/**
 * @file test_ds_malloc.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for malloc function behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-malloc.cpp (Malloc_Tests and
 * Malloc_NoFixture_Tests).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

/** Maximal deviation between calculated and read memory usage. */
static const uint8_t max_memory_usage_deviation = 2u;

static dynostatic_buffer_t buf_;

void setUp(void)
{
    DsBufferTestSetUp(&buf_);
}

void tearDown(void)
{
    DsBufferTestTearDown(&buf_);
}

/*--------------- Fixture-free tests (uninitialized buffer) ---------------*/

void test_Malloc_UnInitialized(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_malloc(&ds_buffer, (void **)&pointer, 5));
}

void test_Malloc_Null_Buffer(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_malloc(NULL, (void **)&pointer, 5));
}

/*--------------------------- Fixture tests ---------------------------*/

void test_Malloc_Bad_Input_Params(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_malloc(&buf_, NULL, 5));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_malloc(&buf_, (void **)&pointer, 0));
}

void test_Malloc_To_Big_Chunk(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_TOO_BIG_CHUNK,
                           ds_malloc(&buf_, (void **)&pointer, DS_MAX_ALLOCATION_SIZE + 1));
}

void test_Malloc_No_NULL(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer, 5));
    TEST_ASSERT_NOT_NULL(pointer);
}

void test_Malloc_Too_Many_Times(void)
{
    char *pointer = NULL;
    unsigned int iter;

    /* Premise guard: allocator slots must run out BEFORE memory does,
     * otherwise this test would fail with ERROR_DS_NO_MEMORY instead. */
    TEST_ASSERT_LESS_OR_EQUAL_size_t((size_t)DS_BUFFER_MEMORY_SIZE,
                                     DS_MAX_ALLOCATION_COUNT * DS_TEST_ALIGN_UP(1u));

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer, 1),
                                       DS_TEST_MSG("iteration %u", iter));
        pointer = NULL;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_ALLOCATORS, ds_malloc(&buf_, (void **)&pointer, 1));
}

void test_Malloc_Lack_Of_Memory(void)
{
    char *pointer = NULL;
    const size_t chunk = DS_TEST_ALIGN_UP(DS_MAX_ALLOCATION_SIZE);
    const size_t full_chunks = DS_BUFFER_MEMORY_SIZE / chunk;
    size_t iter;

    /* Premise guard: memory must run out BEFORE allocator slots do. */
    TEST_ASSERT_LESS_THAN_size_t((size_t)DS_MAX_ALLOCATION_COUNT, full_chunks);

    for (iter = 0u; iter < full_chunks; iter++) {
        TEST_ASSERT_EQUAL_UINT_MESSAGE(
            ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer, DS_MAX_ALLOCATION_SIZE),
            DS_TEST_MSG("iteration %zu", iter));
        pointer = NULL;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY,
                           ds_malloc(&buf_, (void **)&pointer, DS_MAX_ALLOCATION_SIZE));
}

void test_Malloc_Proper_Size(void)
{
    char *pointer = NULL;
    uint8_t r_memory_usage = 0u;
    const uint8_t c_memory_usage = ExpectedUsage(DS_TEST_ALIGN_UP(DS_MAX_ALLOCATION_SIZE));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK,
                           ds_malloc(&buf_, (void **)&pointer, DS_MAX_ALLOCATION_SIZE));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &r_memory_usage));

    TEST_PRINTF("[INFO] Calculated usage: %u Read memory usage: %u", (unsigned int)c_memory_usage,
                (unsigned int)r_memory_usage);

    TEST_ASSERT_INT_WITHIN(max_memory_usage_deviation, c_memory_usage, r_memory_usage);
}

void test_Malloc_Proper_Allocators(void)
{
    char *first_pointer = NULL;
    char *second_pointer = NULL;
    const size_t allocation_len = 10u;
    size_t ptr_diff;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&first_pointer, allocation_len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&second_pointer, allocation_len));

    ptr_diff = (size_t)(second_pointer - first_pointer);

    TEST_PRINTF("[INFO] Distance in memory of two pointers: %u bytes", (unsigned int)ptr_diff);

    /* Physical stride is the ALIGNED size; with the DS_ALIGNMENT == 1
     * fallback this degrades to the raw length (pre-alignment behaviour). */
    TEST_ASSERT_EQUAL_size_t(DS_TEST_ALIGN_UP(allocation_len), ptr_diff);
}

/* Behavioural catcher for the wrong-pointer-return bug: patterns written to
 * two adjacent blocks must not clobber each other, and each block must be
 * readable back in full. */
void test_Malloc_Blocks_Do_Not_Overlap(void)
{
    uint8_t *p1 = NULL;
    uint8_t *p2 = NULL;
    const size_t len = 16u;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, len));

    (void)memset(p1, 0xA5, len);
    (void)memset(p2, 0x5A, len);

    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5, p1[i], DS_TEST_MSG("block 1 clobbered at byte %zu", i));
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5A, p2[i], DS_TEST_MSG("block 2 clobbered at byte %zu", i));
    }
}
