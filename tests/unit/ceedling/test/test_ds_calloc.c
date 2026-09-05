/**
 * @file test_ds_calloc.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for calloc function behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-calloc.cpp (Calloc_Tests and
 * Calloc_NoFixture_Tests).
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

/*--------------- Fixture-free tests (uninitialized buffer) ---------------*/

void test_Calloc_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_calloc(&ds_buffer, (void **)&p, 4, 4));
}

void test_Calloc_Null_Buffer(void)
{
    char *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_calloc(NULL, (void **)&p, 4, 4));
}

/*--------------------------- Fixture tests ---------------------------*/

void test_Calloc_Bad_Args(void)
{
    char *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_calloc(&buf_, NULL, 4, 4));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_calloc(&buf_, (void **)&p, 0, 4));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_calloc(&buf_, (void **)&p, 4, 0));
}

void test_Calloc_Overflow_Is_Rejected(void)
{
    char *p = NULL;
    const size_t huge = (SIZE_MAX / 2u) + 1u; /* huge * 4 overflows size_t */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_calloc(&buf_, (void **)&p, huge, 4));
}

void test_Calloc_Total_Size_Too_Big(void)
{
    char *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_TOO_BIG_CHUNK,
                           ds_calloc(&buf_, (void **)&p, DS_MAX_ALLOCATION_SIZE, 2));
}

void test_Calloc_Memory_Is_Zeroed(void)
{
    uint8_t *p = NULL;
    const size_t len = 8u;
    const size_t elem = 4u;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_calloc(&buf_, (void **)&p, len, elem));
    TEST_ASSERT_NOT_NULL(p);

    for (i = 0u; i < (len * elem); i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, p[i], DS_TEST_MSG("byte %zu not zeroed", i));
    }
}

void test_Calloc_Usage_Reflects_Total_Size(void)
{
    uint8_t *p = NULL;
    const size_t len = 8u;
    const size_t elem = 4u;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_calloc(&buf_, (void **)&p, len, elem));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(DS_TEST_ALIGN_UP(len * elem)), usage);
}

void test_Calloc_Reused_Block_Is_Zeroed(void)
{
    uint8_t *p = NULL;
    char *guard = NULL;
    const size_t len = 16u;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&guard, 8));

    (void)memset(p, 0xFF, len); /* dirty the block before freeing */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_calloc(&buf_, (void **)&p, len, 1));
    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, p[i], DS_TEST_MSG("reused byte %zu not zeroed", i));
    }

    /* The guard block must be untouched by calloc's zeroing.
     * (guard was never written, so verify via a written pattern instead:) */
}

void test_Calloc_Zeroing_Does_Not_Spill_Into_Neighbour(void)
{
    uint8_t *first = NULL;
    uint8_t *zeroed = NULL;
    const size_t len = 16u;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&first, len));
    (void)memset(first, 0xEE, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_calloc(&buf_, (void **)&zeroed, len, 1));

    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(
            0xEE, first[i], DS_TEST_MSG("neighbour byte %zu clobbered by calloc zeroing", i));
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, zeroed[i], DS_TEST_MSG("calloc byte %zu not zeroed", i));
    }
}
