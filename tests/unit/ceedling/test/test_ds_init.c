/**
 * @file test_ds_init.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for initialization and deinitialization of dynostatic buffer.
 *
 * Ceedling/Unity port of tests/unit/utests-init.cpp (Initialization_Tests).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

/* Every test here brings up its own buffer, so there is no shared fixture. */
void setUp(void)
{
}

void tearDown(void)
{
}

void test_Initialization_Invalid_Args(void)
{
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_initialize_allocation(NULL));
}

void test_Initialization_Twice_Initialize(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_ALREADY_INIT, ds_initialize_allocation(&ds_buffer));
    (void)ds_deinit_allocation(&ds_buffer);
}

void test_Initialization_Deinit_Then_Reinit(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_deinit_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_deinit_allocation(&ds_buffer));
}

void test_Initialization_Deinit_Without_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_deinit_allocation(&ds_buffer));
}

void test_Initialization_Deinit_Null_Buffer(void)
{
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_deinit_allocation(NULL));
}

/* Behavioural check that a full deinit/init cycle really resets allocator
 * state: after the cycle the whole buffer must be allocatable again. */
void test_Initialization_Reinit_Restores_Full_Capacity(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&ds_buffer, (void **)&p, DS_MAX_ALLOCATION_SIZE));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_deinit_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(&ds_buffer));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&ds_buffer, &usage));
    TEST_ASSERT_EQUAL_UINT8(0u, usage);

    p = NULL;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&ds_buffer, (void **)&p, DS_MAX_ALLOCATION_SIZE));

    (void)ds_deinit_allocation(&ds_buffer);
}

/* TODO: Fix/implement in DS-Buffer and uncomment (implementation bug: init
 * does not reset data_head/used_allocators, deinit does — init must be
 * self-sufficient for structs with garbage content).
 * NOTE: deliberate white-box test — it simulates a non-zeroed struct, which
 * is unreachable through the public API alone.
   void test_Initialization_Init_Resets_Garbage_State(void)
   {
    dynostatic_buffer_t ds_buffer;
    char *p = NULL;

    (void)memset(&ds_buffer, 0xAA, sizeof(ds_buffer));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(&ds_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&ds_buffer, (void **)&p, DS_MAX_ALLOCATION_SIZE));

    (void)ds_deinit_allocation(&ds_buffer);
   }
 */
