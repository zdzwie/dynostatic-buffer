/**
 * @file test_ds_free.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for free function behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-free.cpp (Free_Tests and
 * Free_NoFixture_Tests).
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

void test_Free_Uninitialized(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char tmp = 0;
    char *pointer = &tmp;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_free(&ds_buffer, (void **)&pointer));
}

void test_Free_Null_Buffer(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_free(NULL, (void **)&pointer));
}

/*--------------------------- Fixture tests ---------------------------*/

void test_Free_Bad_Input_Params(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_free(&buf_, NULL));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_free(&buf_, (void **)&pointer));
}

void test_Free_Nulls_The_Pointer(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer, 5));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pointer));
    TEST_ASSERT_NULL(pointer);
}

void test_Free_Twice_Via_Nulled_Pointer(void)
{
    char *pointer = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer, 5));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pointer));
    /* First free nulled the pointer, so this exercises the NULL guard,
     * NOT the real double-free path (see Real_Double_Free_Is_Rejected). */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_free(&buf_, (void **)&pointer));
}

void test_Free_Pointer_Below_DS_Rejected(void)
{
    char test_variable = 5;
    char *pointer = &test_variable;

    /* Local stack variable is outside the buffer; direction is unknown,
     * so this and the test below are only meaningful together with
     * Free_Pointer_Above_DS_Rejected using controlled offsets. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_MEMORY_OUT_OF_DS, ds_free(&buf_, (void **)&pointer));
}

void test_Free_Pointer_Above_DS_Rejected(void)
{
    /* One-past-end is NOT a valid block start either. */
    char *pointer = (char *)&buf_.memory[DS_BUFFER_MEMORY_SIZE];

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_MEMORY_OUT_OF_DS, ds_free(&buf_, (void **)&pointer));
}

void test_Free_Mid_Block_Pointer_Rejected(void)
{
    char *pointer = NULL;
    char *mid = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pointer, 8));

    mid = pointer + 3; /* inside the block, but not a block start */
    TEST_ASSERT_NOT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&mid));

    /* The original block must still be freeable afterwards. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pointer));
}

void test_Free_Releases_Memory(void)
{
    char *p = NULL;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, DS_MAX_ALLOCATION_SIZE));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(0u, usage); /* no live ALLOCATED descriptors -> 0% */
}

void test_Free_Freed_Slot_Is_Reused(void)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char *guard = NULL;
    char *freed_addr = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, 50));
    /* Guard allocation pins the bump head so the next malloc can only
     * succeed at the same address via the REUSE path, not by rewinding. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&guard, 8));

    freed_addr = p1;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p1));

    /* New, smaller request must reuse the just-freed block (same address). */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, 40));
    TEST_ASSERT_EQUAL_PTR(freed_addr, p2);
}

void test_Free_Real_Double_Free_Is_Rejected(void)
{
    /* Unlike Free_Twice_Via_Nulled_Pointer, keep an alias so the second call
     * sees a non-NULL, already-freed address — the real double-free path. */
    char *p = NULL;
    char *alias = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 5));

    alias = p;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p));

    /* Pick whatever code you settle on (MEMORY_OUT_OF_DS / a dedicated one);
     * the contract is simply: a double free must NOT succeed. */
    TEST_ASSERT_NOT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&alias));
}

void test_Free_Cascade_Reclaims_Free_Chain(void)
{
    /* Layout: [A][B][C], freeing B parks DS_FREE, freeing C cascades through B. */
    char *pa = NULL;
    char *pb = NULL;
    char *pc = NULL;
    char *pb_saved = NULL;
    char *p_new = NULL;
    const size_t len = DS_TEST_ALIGN_UP(32u);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pa, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pb, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pc, len));

    pb_saved = pb;                                                     /* save B's address for later check */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pb)); /* parked */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pc)); /* trailing -> cascade */

    /* 2*len exceeds any single freed block's capacity — only the reclaimed
     * bump space can serve it, and it must land where B used to start. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p_new, 2u * len));
    TEST_ASSERT_EQUAL_PTR(pb_saved, p_new);
}

void test_Free_Cascade_Stops_At_Allocated_Block(void)
{
    /* Layout: [A][B][C]; A stays allocated — cascade must stop above it. */
    char *pa = NULL;
    char *pb = NULL;
    char *pc = NULL;
    const size_t len = DS_TEST_ALIGN_UP(32u);
    size_t i;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pa, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pb, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pc, len));
    (void)memset(pa, 0xAB, len);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pb));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pc));

    /* A survives the sweep untouched... */
    for (i = 0u; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xABu, (uint8_t)pa[i], DS_TEST_MSG("byte %zu", i));
    }
    /* ...and is still freeable (its record was not clobbered). */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pa));
}

void test_Free_Full_Drain_Restores_Full_Capacity(void)
{
    /* Free everything in mixed order; the last free must cascade to zero
     * and the whole buffer must be allocatable again. */
    char *pa = NULL;
    char *pb = NULL;
    char *pc = NULL;
    char *p = NULL;
    const size_t len = DS_TEST_ALIGN_UP(32u);
    uint8_t usage = 0xFFu;
    size_t got;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pa, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pb, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&pc, len));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pb));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pa));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&pc)); /* cascades A+B+C */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8(0u, usage);

    /* Behavioural proof of data_head == 0: DS_MAX chunks fill the buffer again. */
    for (got = 0u; got < DS_BUFFER_MEMORY_SIZE; got += DS_MAX_ALLOCATION_SIZE) {
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK,
                               ds_malloc(&buf_, (void **)&p, DS_MAX_ALLOCATION_SIZE));
        p = NULL;
    }
}
