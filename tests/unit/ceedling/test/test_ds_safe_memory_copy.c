/**
 * @file test_ds_safe_memory_copy.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_safe_memory_copy — bounded copy into live blocks.
 *
 * Ceedling/Unity port of tests/unit/utests-safe-memory-copy.cpp
 * (Safe_Memory_Copy_Tests and Safe_Memory_Copy_NoFixture_Tests).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

#define kPrefill 0xEEu
#define kPattern 0x5Au

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

void test_Safe_Memory_Copy_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t src[4] = { 0 };
    uint8_t dst_dummy = 0;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT,
                           ds_safe_memory_copy(&ds_buffer, &dst_dummy, src, sizeof(src)));
}

void test_Safe_Memory_Copy_Bad_Input_Params(void)
{
    uint8_t src[4] = { 0 };
    uint8_t *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, DS_TEST_ALIGN_UP(8u)));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_safe_memory_copy(NULL, p, src, sizeof(src)));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG,
                           ds_safe_memory_copy(&buf_, NULL, src, sizeof(src)));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG,
                           ds_safe_memory_copy(&buf_, p, NULL, sizeof(src)));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_safe_memory_copy(&buf_, p, src, 0u));
}

/*--------------------- Happy paths ---------------------*/

void test_Safe_Memory_Copy_Copies_Into_Block_Start(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u); /* aligned request -> capacity == len */
    uint8_t *p = NULL;
    uint8_t src[8];
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));

    (void)memset(src, kPattern, sizeof(src));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_safe_memory_copy(&buf_, p, src, sizeof(src)));
    for (i = 0u; i < sizeof(src); i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPattern, p[i], DS_TEST_MSG("byte %zu", i));
    }
}

/* Variant-B discriminator: an INTERIOR destination address must work, and
 * the copy must land exactly at that offset — bytes on both sides intact.
 * Offset 3 is deliberately unaligned: interior addresses carry no
 * alignment guarantee. */
void test_Safe_Memory_Copy_Copies_Into_Block_Interior(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    const size_t shift = 3u;
    const size_t copy_len = 4u;
    uint8_t *p = NULL;
    uint8_t src[4];
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    (void)memset(src, kPattern, sizeof(src));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_safe_memory_copy(&buf_, p + shift, src, copy_len));

    for (i = 0u; i < shift; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("prefix byte %zu clobbered", i));
    }
    for (i = shift; i < (shift + copy_len); i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPattern, p[i], DS_TEST_MSG("copied byte %zu", i));
    }
    for (i = shift + copy_len; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("suffix byte %zu clobbered", i));
    }
}

void test_Safe_Memory_Copy_Exact_Fit_To_Block_End(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    uint8_t src[DS_TEST_ALIGN_UP(16u)];

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));

    (void)memset(src, kPattern, sizeof(src));

    /* shift + (len - shift) == capacity: the boundary case that must PASS. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_safe_memory_copy(&buf_, p + shift, src, len - shift));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPattern, p[len - 1u], "last byte of the block not written");
}

/*--------------------- Bounds violations ---------------------*/

void test_Safe_Memory_Copy_Rejects_One_Byte_Past_End(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    uint8_t src[DS_TEST_ALIGN_UP(16u)];
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    (void)memset(src, kPattern, sizeof(src));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY,
                           ds_safe_memory_copy(&buf_, p + shift, src, (len - shift) + 1u));

    /* Rejection must be side-effect free. */
    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("byte %zu modified by a rejected call", i));
    }
}

/* Overflow guard: shift + src_size wrapping around SIZE_MAX must not slip
 * past the bounds check. A naive `shift + size > capacity` check PASSES
 * these inputs — this test exists to keep the overflow-safe form in place. */
void test_Safe_Memory_Copy_Rejects_Wrapping_Size(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    uint8_t *p = NULL;
    uint8_t src[1] = { kPattern }; /* never read: the copy must not execute */
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    (void)memset(p, kPrefill, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY, ds_safe_memory_copy(&buf_, p, src, SIZE_MAX));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY,
                           ds_safe_memory_copy(&buf_, p + 3, src, SIZE_MAX - 2u));

    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p[i],
                                       DS_TEST_MSG("byte %zu modified by a rejected call", i));
    }
}

/*--------------------- Lookup outcome map ---------------------*/

void test_Safe_Memory_Copy_Rejects_Foreign_Pointer(void)
{
    uint8_t src[4] = { 0 };
    uint8_t stack_var = 0;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_MEMORY_OUT_OF_DS,
                           ds_safe_memory_copy(&buf_, &stack_var, src, sizeof(src)));
}

void test_Safe_Memory_Copy_Rejects_Pointer_In_Parked_Block(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    uint8_t *p = NULL;
    uint8_t *guard = NULL;
    uint8_t *alias = NULL;
    uint8_t src[4] = { 0 };

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&guard, 1u)); /* pins the head */

    alias = p;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p)); /* parked DS_FREE */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALLOCATOR_NOT_FOUND,
                           ds_safe_memory_copy(&buf_, alias, src, sizeof(src)));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALLOCATOR_NOT_FOUND,
                           ds_safe_memory_copy(&buf_, alias + 2, src, sizeof(src)));
}

/* Reclaimed space with a PARTIAL prefix: the containing-scan exits via the
 * break on the first DS_NOT_USED record. */
void test_Safe_Memory_Copy_Rejects_Reclaimed_Space(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    uint8_t *p = NULL;
    uint8_t *alias = NULL;
    uint8_t src[4] = { 0 };

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, len));

    alias = p;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p)); /* trailing -> rollback */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALLOCATOR_NOT_FOUND,
                           ds_safe_memory_copy(&buf_, alias, src, sizeof(src)));
}

/* Past data_head with a FULL prefix: no DS_NOT_USED record exists, so the
 * containing-scan must exit through its natural loop end — the branch
 * that is easiest to leave untested. */
void test_Safe_Memory_Copy_Rejects_Past_Head_With_Full_Prefix(void)
{
    uint8_t *p = NULL;
    uint8_t *last = NULL;
    uint8_t *past_head = NULL;
    uint8_t src[1] = { 0 };
    size_t iter;

    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE(
        (size_t)DS_BUFFER_MEMORY_SIZE,
        ((size_t)DS_MAX_ALLOCATION_COUNT * DS_TEST_ALIGN_UP(1u)) + DS_TEST_ALIGN_UP(4u),
        "premise: arena space must remain after slot exhaustion");

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 1u));
        last = p;
        p = NULL;
    }

    if (NULL == last) {
        TEST_FAIL_MESSAGE("premise: at least one allocation must have succeeded");
    }

    /* One byte past the last block = data_head position: inside the arena
     * (premise guarantees spare space), outside every live block. */
    past_head = last + DS_TEST_ALIGN_UP(1u);
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALLOCATOR_NOT_FOUND,
                           ds_safe_memory_copy(&buf_, past_head, src, sizeof(src)));
}

void test_Safe_Memory_Copy_Does_Not_Touch_Neighbour_Block(void)
{
    const size_t len = DS_TEST_ALIGN_UP(16u);
    uint8_t *p1 = NULL;
    uint8_t *p2 = NULL;
    uint8_t src[DS_TEST_ALIGN_UP(16u)];
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, len));
    (void)memset(p2, kPrefill, len);

    (void)memset(src, kPattern, sizeof(src));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_safe_memory_copy(&buf_, p1, src, len)); /* exact fit */

    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(kPrefill, p2[i],
                                       DS_TEST_MSG("neighbour byte %zu clobbered", i));
    }
}
