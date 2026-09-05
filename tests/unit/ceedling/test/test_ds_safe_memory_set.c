/**
 * @file test_ds_safe_memory_set.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_safe_memory_set — bounded fill of live blocks.
 *
 * Ceedling/Unity port of tests/unit/utests-safe-memory-set.cpp
 * (Safe_Memory_Set_Tests and Safe_Memory_Set_NoFixture_Tests).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

#define kPrefill 0xEEu
#define kValue   0xA5u

static dynostatic_buffer_t buf_;

void setUp(void)
{
    DsBufferTestSetUp(&buf_);
}

void tearDown(void)
{
    DsBufferTestTearDown(&buf_);
}

/*--------------------- Validation paths ---------------------*/

void test_Safe_Memory_Set_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t dst_dummy = 0;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT,
                           ds_safe_memory_set(&ds_buffer, &dst_dummy, (char)kValue, 4u));
}

void test_Safe_Memory_Set_Bad_Input_Params(void)
{
    uint8_t *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, DS_TEST_ALIGN_UP(8u)));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_safe_memory_set(NULL, p, (char)kValue, 4u));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG,
                           ds_safe_memory_set(&buf_, NULL, (char)kValue, 4u));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_safe_memory_set(&buf_, p, (char)kValue, 0u));
}

/*--------------------- Happy paths ---------------------*/

void test_Safe_Memory_Set_Fills_Block_Start_With_Value(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u); /* aligned request -> capacity == len */
    const size_t fill_len = 8u;
    uint8_t *p = NULL;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_safe_memory_set(&buf_, p, (char)kValue, fill_len));

    for (i = 0u; i < fill_len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kValue, p[i], DS_TEST_MSG("byte %zu", i));
    }
    for (i = fill_len; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("byte %zu beyond the fill clobbered", i));
    }
}

/* Variant-B discriminator: fill starting at an (unaligned) interior
 * address; both flanks of the filled range must stay intact. */
void test_Safe_Memory_Set_Fills_Block_Interior(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    const size_t shift = 3u;
    const size_t fill_len = 5u;
    uint8_t *p = NULL;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK,
                           ds_safe_memory_set(&buf_, p + shift, (char)kValue, fill_len));

    for (i = 0u; i < shift; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("prefix byte %zu clobbered", i));
    }
    for (i = shift; i < (shift + fill_len); i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kValue, p[i], DS_TEST_MSG("filled byte %zu", i));
    }
    for (i = shift + fill_len; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("suffix byte %zu clobbered", i));
    }
}

/* Value 0x00 must behave identically — guards against any future "zero
 * means something special" shortcut in the ds_memset family. */
void test_Safe_Memory_Set_Fills_With_Zero_Value(void)
{
    const size_t len = DS_TEST_ALIGN_UP(8u);
    uint8_t *p = NULL;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_safe_memory_set(&buf_, p, (char)0x00, len));
    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, p[i], DS_TEST_MSG("byte %zu", i));
    }
}

void test_Safe_Memory_Set_Exact_Fit_To_Block_End(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));

    /* shift + (len - shift) == capacity: the boundary case that must PASS. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK,
                           ds_safe_memory_set(&buf_, p + shift, (char)kValue, len - shift));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(kValue, p[len - 1u], "last byte of the block not written");
}

/*--------------------- Bounds violations ---------------------*/

void test_Safe_Memory_Set_Rejects_One_Byte_Past_End(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    TEST_ASSERT_EQUAL_UINT(
        ERROR_DS_NO_MEMORY,
        ds_safe_memory_set(&buf_, p + shift, (char)kValue, (len - shift) + 1u));

    /* Rejection must be side-effect free. */
    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("byte %zu modified by a rejected call", i));
    }
}

/* Overflow guard: shift + cnt wrapping around SIZE_MAX must not slip past
 * the bounds check (naive `shift + cnt > capacity` passes these inputs). */
void test_Safe_Memory_Set_Rejects_Wrapping_Count(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    uint8_t *p = NULL;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY,
                           ds_safe_memory_set(&buf_, p, (char)kValue, SIZE_MAX));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY,
                           ds_safe_memory_set(&buf_, p + 3, (char)kValue, SIZE_MAX - 2u));

    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("byte %zu modified by a rejected call", i));
    }
}

/*--------------------- Lookup outcome spot-checks ---------------------*/
/* The full lookup-branch map for ds_find_allocator_containing lives in
 * test_ds_safe_memory_copy.c; here only the dispatch of THIS public
 * function into the lookup is verified. */

void test_Safe_Memory_Set_Rejects_Foreign_Pointer(void)
{
    uint8_t stack_var = 0;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_MEMORY_OUT_OF_DS,
                           ds_safe_memory_set(&buf_, &stack_var, (char)kValue, 1u));
}

void test_Safe_Memory_Set_Rejects_Pointer_In_Parked_Block(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    uint8_t *p = NULL;
    uint8_t *guard = NULL;
    uint8_t *alias = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&guard, 1u)); /* pins the head */

    alias = p;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p)); /* parked DS_FREE */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALLOCATOR_NOT_FOUND,
                           ds_safe_memory_set(&buf_, alias + 2, (char)kValue, 4u));
}
