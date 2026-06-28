/**
 * @file utests-free.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for free function behaviour.
 * @version 0.1
 * @date 2022-07-24
 *
 */

#include <gtest/gtest.h>
#include "logger.hpp"

extern "C" {
#include "dynostatic-buffer.h"
#include "error.h"
}

constexpr uint8_t max_memory_usage_deviation = 2; /**< Maximal deviation between calculated and read memory usage. */

TEST(Multi_Alloc_Tests, Malloc_Few_Times)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer1 = NULL;
    char *pointer2 = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t c_usage = 0;
    uint8_t r_usage = 0;

    ASSERT_TRUE((2 * DS_MAX_ALLOCATION_SIZE) <= DS_BUFFER_MEMORY_SIZE);

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer1), allocation_len), ERROR_DS_OK);

    c_usage = (uint8_t)((((uint16_t)(2ul * allocation_len)) * 100ul) / DS_BUFFER_MEMORY_SIZE);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer2), allocation_len), ERROR_DS_OK);

    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &r_usage), ERROR_DS_OK);

    Logger::info("Calculated usage: " + std::to_string((unsigned int)c_usage)
                 + " Read memory usage: " + std::to_string((unsigned int)r_usage));

    ASSERT_TRUE(abs(r_usage - c_usage) < max_memory_usage_deviation);
    ds_deinit_allocation(&ds_buffer);
}

/* TODO: Fix it and uncomment that test.
   TEST(Multi_Alloc_Tests, Malloc_Twice_Non_Free_Ptr)
   {
    dynostatic_buffer_t ds_buffer = {0};
    char *pointer1 = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer1), allocation_len), ERROR_DS_OK);

    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer1), allocation_len), ERROR_DS_PTR_ALLOC_YET);
    ds_deinit_allocation(&ds_buffer);
   }
 */
