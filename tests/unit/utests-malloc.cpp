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

#include "gtest_out.hpp"

#include "utests-common.hpp"

#include "dynostatic-buffer.h"
#include "ds-defs.h"

#include <cmath>

constexpr uint8_t max_memory_usage_dev = 2; /**< Maximal deviation between calculated and read memory usage. */

TEST(Malloc_Tests, Malloc_UnInitialized)
{
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_NO_INIT);
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);

    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_Bad_Input_Params)
{
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc(NULL, allocation_len), EDS_INVALID_PARAMS);
    ASSERT_EQ(ds_malloc((void **)&pointer, 0), EDS_INVALID_PARAMS);

    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_To_Big_Chunk)
{
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE + 1;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_TOO_BIG_CHUNK);

    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_To_Many_Times)
{
    char *pointer = NULL;
    size_t allocation_len = 1;

    ASSERT_TRUE(DS_MAX_ALLOCATION_COUNT >= DS_MAX_ALLOCATION_SIZE);
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);

    for (unsigned int iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);
    }
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_NO_ALLOCATORS);

    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_No_NULL)
{
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);

    ASSERT_TRUE(pointer != NULL);

    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_Lack_Of_Memory)
{
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    unsigned int allocation_iter = 1;

    ASSERT_EQ(((DS_MAX_ALLOCATION_COUNT * DS_MAX_ALLOCATION_SIZE) >= DS_BUFFER_MEMORY_SIZE), true);
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);

    while ((allocation_iter * DS_MAX_ALLOCATION_SIZE) < DS_BUFFER_MEMORY_SIZE) {
        ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);
        allocation_iter++;
    }

    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_NO_MEMORY);
    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_Proper_Size)
{
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t r_memory_usage = 0;
    uint8_t c_memory_usage = (uint8_t) (DS_MAX_ALLOCATION_SIZE * 100ul / DS_BUFFER_MEMORY_SIZE);

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);

    ASSERT_EQ(ds_get_memory_usage(&r_memory_usage), EDS_OK);

    GOUT.info() << "Calculated usage: " << ((unsigned int) c_memory_usage) << " Read memory usage: " << ((unsigned int) r_memory_usage) << std::endl;

    ASSERT_TRUE(abs(r_memory_usage - c_memory_usage) < max_memory_usage_dev);
    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_Proper_Allocators)
{
    char *first_pointer = NULL;
    char *second_pointer = NULL;
    size_t allocation_len = 10;
    size_t ptr_diff = 0;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);

    ASSERT_EQ(ds_malloc((void **)&first_pointer, allocation_len), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&second_pointer, allocation_len), EDS_OK);

    ptr_diff = (size_t) (second_pointer - first_pointer);

    GOUT.info() << "Distance in memory of two pointers: " << ((unsigned int) ptr_diff) << " bytes" << std::endl;

    ASSERT_EQ(ptr_diff, allocation_len);
    ds_deinit_allocation();
}

TEST(Malloc_Tests, Malloc_Minus)
{
    char *pointer = NULL;
    size_t allocation_len = -5;

    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_INVALID_PARAMS);
    ASSERT_TRUE(pointer != NULL);

    ds_deinit_allocation();
}
