/**
 * @file test_ds_realloc.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for realloc function behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-realloc.cpp (Realloc_Tests and
 * Realloc_NoFixture_Tests).
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

void test_Realloc_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    void *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_realloc(&ds_buffer, &p, 16));
}

void test_Realloc_Null_Buffer(void)
{
    void *p = NULL;

    /* ds_realloc folds the NULL-buffer check into its init check, so a NULL
     * buffer surfaces as ERROR_DS_NO_INIT rather than ERROR_DS_INVALID_ARG. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_realloc(NULL, &p, 16));
}

/*--------------------------- Fixture tests ---------------------------*/

void test_Realloc_Bad_Args(void)
{
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_realloc(&buf_, NULL, 16));
}

void test_Realloc_Null_Acts_As_Malloc(void)
{
    void *p = NULL;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 16));
    TEST_ASSERT_NOT_NULL(p);

    /* Discriminating assertion: the malloc path must actually account for
     * the memory, not just hand back a pointer. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(DS_TEST_ALIGN_UP(16u)), usage);
}

void test_Realloc_Zero_Size_Acts_As_Free(void)
{
    void *p = NULL;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 32));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 0));
    TEST_ASSERT_NULL(p);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(0u, usage);
}

void test_Realloc_Shrink_Keeps_Pointer(void)
{
    void *p = NULL;
    void *before = NULL;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 64));
    before = p;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 16));
    TEST_ASSERT_EQUAL_PTR(before, p); /* shrink in place */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(DS_TEST_ALIGN_UP(64u)), usage);
}

void test_Realloc_Grow_Preserves_Contents(void)
{
    void *p = NULL;
    uint8_t *bytes = NULL;
    uint8_t i;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 10));

    bytes = (uint8_t *)p;
    for (i = 0u; i < 10u; i++) {
        bytes[i] = (uint8_t)(i + 1u);
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 40));

    bytes = (uint8_t *)p;
    for (i = 0u; i < 10u; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)(i + 1u), bytes[i],
                                       DS_TEST_MSG("byte %d not preserved", (int)i));
    }

    /* Discriminating assertions a no-op implementation cannot pass:
     * 1) the grown region must be writable and readable back, */
    (void)memset(bytes + 10, 0xC3, 30);
    for (i = 10u; i < 40u; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xC3, bytes[i],
                                       DS_TEST_MSG("grown byte %d not usable", (int)i));
    }
    /* 2) accounting must reflect the new size. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(DS_TEST_ALIGN_UP(40u)), usage);
}

void test_Realloc_Grow_Moves_When_Blocked(void)
{
    void *p = NULL;
    void *guard = NULL;
    void *before = NULL;
    uint8_t *bytes = NULL;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 16));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, &guard, 16));

    bytes = (uint8_t *)p;
    (void)memset(bytes, 0x77, 16);

    before = p;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 64));
    TEST_ASSERT_TRUE_MESSAGE(p != before, "block must have moved"); /* must have moved */

    bytes = (uint8_t *)p;
    for (i = 0u; i < 16u; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x77, bytes[i],
                                       DS_TEST_MSG("byte %zu lost during move", i));
    }
}

void test_Realloc_Rejects_Too_Big_Size(void)
{
    void *p = NULL;
    void *before = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 16));
    before = p;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_TOO_BIG_CHUNK,
                           ds_realloc(&buf_, &p, DS_MAX_ALLOCATION_SIZE + 1u));
    TEST_ASSERT_EQUAL_PTR(before, p);
}

void test_Realloc_Rejects_Foreign_And_Interior_Pointers(void)
{
    void *p = NULL;
    void *interior = NULL;
    char stack_var = 0;
    void *foreign = &stack_var;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_realloc(&buf_, &p, 16));

    /* Interior pointer: in the arena, not a block start. */
    interior = (uint8_t *)p + 3;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALLOCATOR_NOT_FOUND, ds_realloc(&buf_, &interior, 32));

    /* Foreign pointer: outside the arena entirely. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_MEMORY_OUT_OF_DS, ds_realloc(&buf_, &foreign, 32));
    TEST_ASSERT_EQUAL_PTR(&stack_var, foreign);
}
