/**
 * @file utests-getters.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for diagnostic getters: ds_get_max_new_allocation_size
 *        and ds_get_free_allocator_cnt.
 * @version 0.1
 * @date 2026-07-10
 */
#include "utests-common.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;

class Getter_Tests : public DsBufferTest {
  protected:
    void FillBumpCompletely()
    {
        ASSERT_EQ(DS_BUFFER_MEMORY_SIZE % AlignUp(1u) == 0u
                      ? 0u
                      : DS_BUFFER_MEMORY_SIZE % AlignUp(1u),
                  DS_BUFFER_MEMORY_SIZE % AlignUp(1u)); /* no-op, keeps -Wall quiet on fallback */

        size_t remaining = DS_BUFFER_MEMORY_SIZE;
        size_t chunks = 0u;
        while (remaining > 0u) {
            size_t chunk = remaining;
            if (chunk > DS_MAX_ALLOCATION_SIZE) {
                chunk = DS_MAX_ALLOCATION_SIZE;
            }
            ASSERT_EQ(AlignUp(chunk), chunk)
                << "premise: buffer size must decompose into aligned chunks";
            char *p = NULL;
            ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), chunk), ERROR_DS_OK);
            remaining -= chunk;
            chunks++;
            ASSERT_LE(chunks, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT))
                << "premise: enough allocator slots to fill the bump space";
        }
    }

    void AssertMaxAllocationIsTight()
    {
        size_t v = 0u;
        ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);

        char *p = NULL;
        if (v == 0u) {
            ASSERT_NE(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1u), ERROR_DS_OK);
            return;
        }

        /* Probe the failure first: a failed malloc must not mutate state. */
        ASSERT_NE(ds_malloc(&buf_, reinterpret_cast<void **>(&p), v + 1u), ERROR_DS_OK)
            << "getter reported " << v << " but " << (v + 1u) << " also succeeds";
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), v), ERROR_DS_OK)
            << "getter reported " << v << " but allocating it fails";
    }
};

/*--------------------- Validation paths ---------------------*/

TEST(Getter_NoFixture_Tests, Max_Allocation_No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    size_t v = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&ds_buffer, &v), ERROR_DS_NO_INIT);
}

TEST(Getter_NoFixture_Tests, Free_Allocator_Cnt_No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    size_t v = 0u;
    ASSERT_EQ(ds_get_free_allocator_cnt(&ds_buffer, &v), ERROR_DS_NO_INIT);
}

TEST_F(Getter_Tests, Bad_Input_Params)
{
    size_t v = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(NULL, &v), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, NULL), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_get_free_allocator_cnt(NULL, &v), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, NULL), ERROR_DS_INVALID_ARG);
}

/*--------------- ds_get_free_allocator_cnt ---------------*/

TEST_F(Getter_Tests, Fresh_Buffer_Reports_All_Slots)
{
    size_t cnt = 0u;
    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT));
}

TEST_F(Getter_Tests, Cnt_Tracks_Live_Blocks)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char *p3 = NULL;
    size_t cnt = 0u;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), 1u), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), 1u), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p3), 1u), ERROR_DS_OK);

    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) - 3u);
}

TEST_F(Getter_Tests, Parked_Free_Slot_Counts_As_Free)
{
    char *p = NULL;
    char *guard = NULL;
    size_t cnt = 0u;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 8u), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&guard), 1u), ERROR_DS_OK); /* pins the bump head */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK);           /* parked, not reclaimed */

    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) - 1u); /* only guard is live */
}

TEST_F(Getter_Tests, Reclaimed_Slot_Counts_As_Free)
{
    char *p = NULL;
    size_t cnt = 0u;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 8u), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK); /* trailing -> rollback */

    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT));
}

TEST_F(Getter_Tests, Zero_Cnt_Guarantees_No_Allocators)
{
    ASSERT_LE(static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) * AlignUp(1u),
              static_cast<size_t>(DS_BUFFER_MEMORY_SIZE))
        << "premise: slots must run out before memory does";

    char *p = NULL;
    for (size_t iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1u), ERROR_DS_OK);
        p = NULL;
    }

    size_t cnt = 0xFFu;
    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, 0u);

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1u), ERROR_DS_NO_ALLOCATORS);

    /* Cross-getter invariant: no slots implies no allocation of any size. */
    size_t v = 0xFFu;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);
    ASSERT_EQ(v, 0u);
}

TEST_F(Getter_Tests, Fresh_Buffer_Reports_Clamped_Max)
{
    size_t v = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);
    ASSERT_EQ(v, static_cast<size_t>(DS_MAX_ALLOCATION_SIZE));

    AssertMaxAllocationIsTight();
}

TEST_F(Getter_Tests, Bump_Remainder_Reported_When_Below_Max)
{
    /* Leave less than DS_MAX_ALLOCATION_SIZE in the bump space. */
    const size_t leftover = AlignUp(32u);
    ASSERT_LT(leftover, static_cast<size_t>(DS_MAX_ALLOCATION_SIZE));

    size_t remaining = DS_BUFFER_MEMORY_SIZE;
    char *p = NULL;
    while (remaining > leftover) {
        size_t chunk = remaining - leftover;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        ASSERT_EQ(AlignUp(chunk), chunk) << "premise: aligned decomposition";
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), chunk), ERROR_DS_OK);
        p = NULL;
        remaining -= chunk;
    }

    size_t v = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);
    ASSERT_EQ(v, leftover);

    AssertMaxAllocationIsTight();
}

TEST_F(Getter_Tests, Parked_Block_Reported_When_Bump_Exhausted)
{
    const size_t parked = AlignUp(96u);
    ASSERT_LT(parked, static_cast<size_t>(DS_MAX_ALLOCATION_SIZE));

    /* [A=parked][fill to the end], then free A (non-trailing -> parks). */
    char *pa = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pa), parked), ERROR_DS_OK);

    size_t remaining = DS_BUFFER_MEMORY_SIZE - parked;
    char *p = NULL;
    while (remaining > 0u) {
        size_t chunk = remaining;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        ASSERT_EQ(AlignUp(chunk), chunk) << "premise: aligned decomposition";
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), chunk), ERROR_DS_OK);
        p = NULL;
        remaining -= chunk;
    }

    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pa)), ERROR_DS_OK);

    size_t v = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);
    ASSERT_EQ(v, parked);

    AssertMaxAllocationIsTight();
}

TEST_F(Getter_Tests, Zero_When_Slots_Exhausted_With_Memory_Left)
{
    ASSERT_LE(static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) * AlignUp(1u) + AlignUp(1u),
              static_cast<size_t>(DS_BUFFER_MEMORY_SIZE))
        << "premise: memory must remain after slot exhaustion";

    char *p = NULL;
    for (size_t iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1u), ERROR_DS_OK);
        p = NULL;
    }

    size_t v = 0xFFu;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);
    ASSERT_EQ(v, 0u);

    AssertMaxAllocationIsTight();
}

TEST_F(Getter_Tests, Full_Prefix_Reports_Largest_Parked_Capacity)
{
    const size_t big = AlignUp(8u);
    const size_t small = AlignUp(1u);
    ASSERT_GT(big, small) << "premise: distinct capacities";
    ASSERT_LE(big + (static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) - 1u) * small,
              static_cast<size_t>(DS_BUFFER_MEMORY_SIZE))
        << "premise: layout fits the buffer";

    char *blocks[DS_MAX_ALLOCATION_COUNT] = { NULL };
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&blocks[0]), big), ERROR_DS_OK);
    for (size_t iter = 1u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&blocks[iter]), small), ERROR_DS_OK);
    }

    /* Free two non-trailing blocks of different capacities. */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&blocks[0])), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&blocks[1])), ERROR_DS_OK);

    size_t v = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &v), ERROR_DS_OK);
    ASSERT_EQ(v, big);

    AssertMaxAllocationIsTight();
}
