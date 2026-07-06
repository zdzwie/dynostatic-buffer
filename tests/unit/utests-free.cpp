/**
 * @file utests-free.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for free function behaviour.
 * @version 0.2
 * @date 2026-07-03
 */
#include "utests-common.hpp"

using dstest::DsBufferTest;

class Free_Tests : public DsBufferTest {};

TEST(Free_NoFixture_Tests, Free_Uninitialized)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char tmp;
    char *pointer = &tmp;

    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_NO_INIT);
}

TEST(Free_NoFixture_Tests, Free_Null_Buffer)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_free(NULL, reinterpret_cast<void **>(&pointer)), ERROR_DS_INVALID_ARG);
}

TEST_F(Free_Tests, Free_Bad_Input_Params)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_free(&buf_, NULL), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_INVALID_ARG);
}

TEST_F(Free_Tests, Free_Nulls_The_Pointer)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 5), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
    ASSERT_TRUE(pointer == NULL);
}

TEST_F(Free_Tests, Free_Twice_Via_Nulled_Pointer)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 5), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
    /* First free nulled the pointer, so this exercises the NULL guard,
     * NOT the real double-free path (see Real_Double_Free_Is_Rejected). */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_INVALID_ARG);
}

TEST_F(Free_Tests, Free_Pointer_Below_DS_Rejected)
{
    char test_variable = 5;
    char *pointer = &test_variable;

    // Local stack variable is outside the buffer; direction is unknown,
    // so this and the test below are only meaningful together with
    // Free_Pointer_Above_DS_Rejected using controlled offsets.
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_MEMORY_OUT_OF_DS);
}

TEST_F(Free_Tests, Free_Pointer_Above_DS_Rejected)
{
    // One-past-end is NOT a valid block start either.
    char *pointer = reinterpret_cast<char *>(&buf_.memory[DS_BUFFER_MEMORY_SIZE]);

    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_MEMORY_OUT_OF_DS);
}

TEST_F(Free_Tests, Free_Mid_Block_Pointer_Rejected)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 8), ERROR_DS_OK);

    char *mid = pointer + 3; // inside the block, but not a block start
    ASSERT_NE(ds_free(&buf_, reinterpret_cast<void **>(&mid)), ERROR_DS_OK);

    // The original block must still be freeable afterwards.
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
}

TEST_F(Free_Tests, Free_Releases_Memory)
{
    char *p = NULL;
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK);

    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0); // no live ALLOCATED descriptors -> 0%
}

TEST_F(Free_Tests, Freed_Slot_Is_Reused)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char *guard = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), 50), ERROR_DS_OK);
    // Guard allocation pins the bump head so the next malloc can only
    // succeed at the same address via the REUSE path, not by rewinding.
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&guard), 8), ERROR_DS_OK);

    char *freed_addr = p1;
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p1)), ERROR_DS_OK);

    // New, smaller request must reuse the just-freed block (same address).
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), 40), ERROR_DS_OK);
    ASSERT_EQ(p2, freed_addr);
}

TEST_F(Free_Tests, Real_Double_Free_Is_Rejected)
{
    // Unlike Free_Twice_Via_Nulled_Pointer, keep an alias so the second call
    // sees a non-NULL, already-freed address — the real double-free path.
    char *p = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 5), ERROR_DS_OK);

    char *alias = p;
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK);

    // Pick whatever code you settle on (MEMORY_OUT_OF_DS / a dedicated one);
    // the contract is simply: a double free must NOT succeed.
    ASSERT_NE(ds_free(&buf_, reinterpret_cast<void **>(&alias)), ERROR_DS_OK);
}

TEST_F(Free_Tests, Cascade_Reclaims_Free_Chain)
{
    /* Layout: [A][B][C], freeing B parks DS_FREE, freeing C cascades through B. */
    char *pa = NULL;
    char *pb = NULL;
    char *pc = NULL;
    const size_t len = dstest::AlignUp(32u);

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pa), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pb), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pc), len), ERROR_DS_OK);

    char *pb_saved = pb;                                                    /* save B's address for later check */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pb)), ERROR_DS_OK); /* parked */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pc)), ERROR_DS_OK); /* trailing -> cascade */

    /* 2*len exceeds any single freed block's capacity — only the reclaimed
     * bump space can serve it, and it must land where B used to start. */
    char *p_new = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p_new), 2u * len), ERROR_DS_OK);
    ASSERT_EQ(p_new, pb_saved); /* keep a saved copy of pb before freeing */
}

TEST_F(Free_Tests, Cascade_Stops_At_Allocated_Block)
{
    /* Layout: [A][B][C]; A stays allocated — cascade must stop above it. */
    char *pa = NULL;
    char *pb = NULL;
    char *pc = NULL;
    const size_t len = dstest::AlignUp(32u);

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pa), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pb), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pc), len), ERROR_DS_OK);
    std::memset(pa, 0xAB, len);

    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pb)), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pc)), ERROR_DS_OK);

    /* A survives the sweep untouched... */
    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(static_cast<uint8_t>(pa[i]), 0xABu) << "byte " << i;
    }
    /* ...and is still freeable (its record was not clobbered). */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pa)), ERROR_DS_OK);
}

TEST_F(Free_Tests, Full_Drain_Restores_Full_Capacity)
{
    /* Free everything in mixed order; the last free must cascade to zero
     * and the whole buffer must be allocatable again. */
    char *pa = NULL;
    char *pb = NULL;
    char *pc = NULL;
    const size_t len = dstest::AlignUp(32u);

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pa), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pb), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pc), len), ERROR_DS_OK);

    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pb)), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pa)), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&pc)), ERROR_DS_OK); /* cascades A+B+C */

    uint8_t usage = 0xFF;
    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0u);

    /* Behavioural proof of data_head == 0: DS_MAX chunks fill the buffer again. */
    char *p = NULL;
    for (size_t got = 0; got < DS_BUFFER_MEMORY_SIZE; got += DS_MAX_ALLOCATION_SIZE) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);
        p = NULL;
    }
}
