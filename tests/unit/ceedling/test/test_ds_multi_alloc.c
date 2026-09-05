/**
 * @file test_ds_multi_alloc.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for multi-allocation behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-multi-alloc.cpp (Multi_Alloc_Tests).
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

void test_Multi_Alloc_Malloc_Few_Times(void)
{
    char *pointer1 = NULL;
    char *pointer2 = NULL;
    const size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t r_usage = 0u;
    uint8_t c_usage = 0u;

    TEST_ASSERT_LESS_OR_EQUAL_size_t((size_t)DS_BUFFER_MEMORY_SIZE,
                                     2u * DS_TEST_ALIGN_UP(allocation_len));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer1, allocation_len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer2, allocation_len));

    /* size_t arithmetic end-to-end — the old uint16_t cast silently
     * truncated for larger buffer configurations. */
    c_usage = ExpectedUsage(2u * DS_TEST_ALIGN_UP(allocation_len));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &r_usage));

    TEST_PRINTF("[INFO] Calculated usage: %u Read memory usage: %u", (unsigned int)c_usage,
                (unsigned int)r_usage);

    TEST_ASSERT_INT_WITHIN(max_memory_usage_deviation, c_usage, r_usage);
}

/* Interleaved pattern integrity across several allocations — behavioural
 * catcher for wrong-pointer-return and head-accounting bugs at once. */
void test_Multi_Alloc_Interleaved_Blocks_Keep_Contents(void)
{
#define kBlocks 4u
#define kLen    8u
    uint8_t *blocks[kBlocks] = { NULL };
    size_t b;
    size_t i;

    for (b = 0u; b < kBlocks; b++) {
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&blocks[b], kLen));
        (void)memset(blocks[b], (int)(0x10u * (b + 1u)), kLen);
    }

    for (b = 0u; b < kBlocks; b++) {
        for (i = 0u; i < kLen; i++) {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)(0x10u * (b + 1u)), blocks[b][i],
                                           DS_TEST_MSG("block %zu byte %zu clobbered", b, i));
        }
    }
#undef kBlocks
#undef kLen
}

void test_Multi_Alloc_Malloc_Twice_Non_Free_Ptr(void)
{
    char *pointer1 = NULL;
    const size_t allocation_len = DS_MAX_ALLOCATION_SIZE;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer1, allocation_len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_PTR_ALLOC_YET,
                           ds_malloc(&buf_, (void **)&pointer1, allocation_len));
}
