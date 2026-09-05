/**
 * @file test_ds_memory_usage.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_get_memory_usage function behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-memory-usage.cpp
 * (MemoryUsage_Tests and MemoryUsage_NoFixture_Tests).
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

void test_MemoryUsage_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_get_memory_usage(&ds_buffer, &usage));
}

void test_MemoryUsage_Null_Buffer(void)
{
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_memory_usage(NULL, &usage));
}

/*--------------------------- Fixture tests ---------------------------*/

void test_MemoryUsage_Null_Output(void)
{
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_memory_usage(&buf_, NULL));
}
