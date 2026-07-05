/**
 * @file utests-memory-usage.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_get_memory_usage function behaviour.
 * @version 0.1
 * @date 2026-07-05
 */
#include "utests-common.hpp"

using dstest::DsBufferTest;

class MemoryUsage_Tests : public DsBufferTest {};

TEST(MemoryUsage_NoFixture_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &usage), ERROR_DS_NO_INIT);
}

TEST(MemoryUsage_NoFixture_Tests, Null_Buffer)
{
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_get_memory_usage(NULL, &usage), ERROR_DS_INVALID_ARG);
}

TEST_F(MemoryUsage_Tests, Null_Output)
{
    ASSERT_EQ(ds_get_memory_usage(&buf_, NULL), ERROR_DS_INVALID_ARG);
}
