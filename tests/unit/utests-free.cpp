/**
 * @file utests-free.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for free function behaviour.
 * @version 0.1
 * @date 2022-07-17
 *
 */

#include <gtest/gtest.h>

#include "gtest_out.hpp"

#include "utests-common.hpp"

#include "dynostatic-buffer.h"
#include "ds-defs.h"

TEST(Free_Tests, Free_Uninitialized)
{
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_free((void **)&pointer), EDS_NO_INIT);
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), EDS_OK);

    ds_deinit_allocation();
}

TEST(Free_Tests, Free_Bad_Input_Params)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), EDS_INVALID_PARAMS);

    ds_deinit_allocation();
}

TEST(Free_Tests, Free_Pointer_Outside_DS)
{
    char test_variable = 5;
    char *pointer = &test_variable;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), EDS_MEMORY_OUT_OF_DS);

    ds_deinit_allocation();
}

TEST(Free_Tests, Freed_Ptr_Is_Null)
{
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), EDS_OK);

    ASSERT_TRUE(pointer == NULL);
    ds_deinit_allocation();
}

TEST(Free_Tests, Freed_Twice)
{
    char *pointer = NULL;
    size_t allocation_len = 5;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), EDS_OK);

    ASSERT_EQ(ds_free((void **)&pointer), EDS_INVALID_PARAMS);
    ds_deinit_allocation();
}

TEST(Free_Tests, Free_Memory_Unlocked)
{
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t memory_usage = 0;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), EDS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), EDS_OK);

    ASSERT_EQ(ds_get_memory_usage(&memory_usage), EDS_OK);
    GOUT.info() << " Read memory usage: " << ((unsigned int) memory_usage) << std::endl;
    ASSERT_EQ(memory_usage, 0);
    ds_deinit_allocation();
}
