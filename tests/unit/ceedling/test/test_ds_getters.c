/**
 * @file test_ds_getters.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for diagnostic getters: ds_get_max_new_allocation_size
 *        and ds_get_free_allocator_cnt.
 *
 * Ceedling/Unity port of tests/unit/utests-getters.cpp (Getter_Tests and
 * Getter_NoFixture_Tests).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

static dynostatic_buffer_t buf_;

void setUp(void)
{
    DsBufferTestSetUp(&buf_);
}

void tearDown(void)
{
    DsBufferTestTearDown(&buf_);
}

/**
 * @brief Assert that ds_get_max_new_allocation_size() reports a TIGHT bound:
 *        the reported size must allocate and one byte more must not.
 */
static void AssertMaxAllocationIsTight(void)
{
    size_t v = 0u;
    char *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));

    if (v == 0u) {
        TEST_ASSERT_NOT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 1u));
        return;
    }

    /* Probe the failure first: a failed malloc must not mutate state. */
    TEST_ASSERT_NOT_EQUAL_UINT_MESSAGE(
        ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, v + 1u),
        DS_TEST_MSG("getter reported %zu but %zu also succeeds", v, v + 1u));
    TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, v),
                                   DS_TEST_MSG("getter reported %zu but allocating it fails", v));
}

/*--------------------- Validation paths ---------------------*/

void test_Getter_Max_Allocation_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    size_t v = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_get_max_new_allocation_size(&ds_buffer, &v));
}

void test_Getter_Free_Allocator_Cnt_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    size_t v = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_get_free_allocator_cnt(&ds_buffer, &v));
}

void test_Getter_Bad_Input_Params(void)
{
    size_t v = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_max_new_allocation_size(NULL, &v));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_max_new_allocation_size(&buf_, NULL));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_free_allocator_cnt(NULL, &v));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_free_allocator_cnt(&buf_, NULL));
}

/*--------------- ds_get_free_allocator_cnt ---------------*/

void test_Getter_Fresh_Buffer_Reports_All_Slots(void)
{
    size_t cnt = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t((size_t)DS_MAX_ALLOCATION_COUNT, cnt);
}

void test_Getter_Cnt_Tracks_Live_Blocks(void)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char *p3 = NULL;
    size_t cnt = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, 1u));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, 1u));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p3, 1u));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t((size_t)DS_MAX_ALLOCATION_COUNT - 3u, cnt);
}

void test_Getter_Parked_Free_Slot_Counts_As_Free(void)
{
    char *p = NULL;
    char *guard = NULL;
    size_t cnt = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 8u));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&guard, 1u)); /* pins the bump head */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p));           /* parked, not reclaimed */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t((size_t)DS_MAX_ALLOCATION_COUNT - 1u, cnt); /* only guard is live */
}

void test_Getter_Reclaimed_Slot_Counts_As_Free(void)
{
    char *p = NULL;
    size_t cnt = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 8u));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p)); /* trailing -> rollback */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t((size_t)DS_MAX_ALLOCATION_COUNT, cnt);
}

void test_Getter_Zero_Cnt_Guarantees_No_Allocators(void)
{
    char *p = NULL;
    size_t cnt = 0xFFu;
    size_t v = 0xFFu;
    size_t iter;

    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE((size_t)DS_BUFFER_MEMORY_SIZE,
                                             (size_t)DS_MAX_ALLOCATION_COUNT * DS_TEST_ALIGN_UP(1u),
                                             "premise: slots must run out before memory does");

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 1u));
        p = NULL;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t(0u, cnt);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_ALLOCATORS, ds_malloc(&buf_, (void **)&p, 1u));

    /* Cross-getter invariant: no slots implies no allocation of any size. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));
    TEST_ASSERT_EQUAL_size_t(0u, v);
}

/*--------------- ds_get_max_new_allocation_size ---------------*/

void test_Getter_Fresh_Buffer_Reports_Clamped_Max(void)
{
    size_t v = 0u;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));
    TEST_ASSERT_EQUAL_size_t((size_t)DS_MAX_ALLOCATION_SIZE, v);

    AssertMaxAllocationIsTight();
}

void test_Getter_Bump_Remainder_Reported_When_Below_Max(void)
{
    /* Leave less than DS_MAX_ALLOCATION_SIZE in the bump space. */
    const size_t leftover = DS_TEST_ALIGN_UP(32u);
    size_t remaining = DS_BUFFER_MEMORY_SIZE;
    char *p = NULL;
    size_t v = 0u;

    TEST_ASSERT_LESS_THAN_size_t((size_t)DS_MAX_ALLOCATION_SIZE, leftover);

    while (remaining > leftover) {
        size_t chunk = remaining - leftover;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        TEST_ASSERT_EQUAL_size_t_MESSAGE(chunk, DS_TEST_ALIGN_UP(chunk),
                                         "premise: aligned decomposition");
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, chunk));
        p = NULL;
        remaining -= chunk;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));
    TEST_ASSERT_EQUAL_size_t(leftover, v);

    AssertMaxAllocationIsTight();
}

void test_Getter_Parked_Block_Reported_When_Bump_Exhausted(void)
{
    const size_t parked = DS_TEST_ALIGN_UP(96u);
    /* [A=parked][fill to the end], then free A (non-trailing -> parks). */
    char *pa = NULL;
    char *p = NULL;
    size_t remaining;
    size_t v = 0u;

    TEST_ASSERT_LESS_THAN_size_t((size_t)DS_MAX_ALLOCATION_SIZE, parked);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pa, parked));

    remaining = DS_BUFFER_MEMORY_SIZE - parked;
    while (remaining > 0u) {
        size_t chunk = remaining;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        TEST_ASSERT_EQUAL_size_t_MESSAGE(chunk, DS_TEST_ALIGN_UP(chunk),
                                         "premise: aligned decomposition");
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, chunk));
        p = NULL;
        remaining -= chunk;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pa));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));
    TEST_ASSERT_EQUAL_size_t(parked, v);

    AssertMaxAllocationIsTight();
}

void test_Getter_Zero_When_Slots_Exhausted_With_Memory_Left(void)
{
    char *p = NULL;
    size_t v = 0xFFu;
    size_t iter;

    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE(
        (size_t)DS_BUFFER_MEMORY_SIZE,
        ((size_t)DS_MAX_ALLOCATION_COUNT * DS_TEST_ALIGN_UP(1u)) + DS_TEST_ALIGN_UP(1u),
        "premise: memory must remain after slot exhaustion");

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 1u));
        p = NULL;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));
    TEST_ASSERT_EQUAL_size_t(0u, v);

    AssertMaxAllocationIsTight();
}

void test_Getter_Full_Prefix_Reports_Largest_Parked_Capacity(void)
{
    const size_t big = DS_TEST_ALIGN_UP(8u);
    const size_t small = DS_TEST_ALIGN_UP(1u);
    char *blocks[DS_MAX_ALLOCATION_COUNT] = { NULL };
    size_t iter;
    size_t v = 0u;

    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(small, big, "premise: distinct capacities");
    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE(
        (size_t)DS_BUFFER_MEMORY_SIZE, big + (((size_t)DS_MAX_ALLOCATION_COUNT - 1u) * small),
        "premise: layout fits the buffer");

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&blocks[0], big));
    for (iter = 1u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&blocks[iter], small));
    }

    /* Free two non-trailing blocks of different capacities. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&blocks[0]));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&blocks[1]));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &v));
    TEST_ASSERT_EQUAL_size_t(big, v);

    AssertMaxAllocationIsTight();
}
