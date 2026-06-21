/**
 * @file utests-malloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for malloc function behaviour.
 * @version 0.1
 * @date 2022-07-17
 *
 */

 #include <gtest/gtest.h>
 #include "logger.hpp"

extern "C" {
     #include "dynostatic-buffer.h"
     #include "error.h"
}

#include <cmath>

constexpr uint8_t max_memory_usage_deviation = 2; /**< Maximal deviation between calculated and read memory usage. */

TEST(Malloc_Tests, Malloc_UnInitialized)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_NO_INIT);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);

    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_Bad_Input_Params)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, NULL, allocation_len), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), 0), ERROR_DS_INVALID_ARG);

    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_To_Big_Chunk)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE + 1;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_TOO_BIG_CHUNK);

    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_Too_Many_Times)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 1;

    ASSERT_TRUE((DS_MAX_ALLOCATION_COUNT * DS_MAX_ALLOCATION_SIZE) >= DS_BUFFER_MEMORY_SIZE);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);

    for (unsigned int iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);
    }

    Logger::info("Allocated ", ds_buffer.used_allocators, " times.");
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_NO_ALLOCATORS);

    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_No_NULL)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);

    ASSERT_TRUE(pointer != NULL);

    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_Lack_Of_Memory)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    unsigned int allocation_iter = 1;

    ASSERT_EQ(((DS_MAX_ALLOCATION_COUNT * DS_MAX_ALLOCATION_SIZE) >= DS_BUFFER_MEMORY_SIZE), true);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);

    while ((allocation_iter * DS_MAX_ALLOCATION_SIZE) <= DS_BUFFER_MEMORY_SIZE) {
        ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);
        allocation_iter++;
    }

    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_NO_MEMORY);
    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_Proper_Size)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t r_memory_usage = 0;
    uint8_t c_memory_usage = (uint8_t) (DS_MAX_ALLOCATION_SIZE * 100ul / DS_BUFFER_MEMORY_SIZE);

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);

    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &r_memory_usage), ERROR_DS_OK);

    Logger::info("Calculated usage: ", ((unsigned int) c_memory_usage), " Read memory usage: ", ((unsigned int) r_memory_usage));

    ASSERT_TRUE(abs(r_memory_usage - c_memory_usage) < max_memory_usage_deviation);
    ds_deinit_allocation(&ds_buffer);
}

TEST(Malloc_Tests, Malloc_Proper_Allocators)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *first_pointer = NULL;
    char *second_pointer = NULL;
    size_t allocation_len = 10;
    size_t ptr_diff = 0;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);

    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&first_pointer), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&second_pointer), allocation_len), ERROR_DS_OK);

    ptr_diff = (size_t) (second_pointer - first_pointer);

    Logger::info("Distance in memory of two pointers: ", (unsigned int)ptr_diff, " bytes");

    ASSERT_EQ(ptr_diff, allocation_len);
    ds_deinit_allocation(&ds_buffer);
}
