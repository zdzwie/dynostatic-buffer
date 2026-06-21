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
#include "logger.hpp"

extern "C" {
    #include "dynostatic-buffer.h"
    #include "error.h"
}

TEST(Free_Tests, Free_Uninitialized)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;
    ASSERT_EQ(ds_free(&ds_buffer, (void **)&pointer), ERROR_DS_NO_INIT);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, (void **)&pointer, allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, (void **)&pointer), ERROR_DS_OK);
    ds_deinit_allocation(&ds_buffer);
}
TEST(Free_Tests, Free_Bad_Input_Params)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, (void **)&pointer), ERROR_DS_INVALID_ARG);
    ds_deinit_allocation(&ds_buffer);
}
/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Free_Pointer_Outside_DS)
   {
    char test_variable = 5;
    char *pointer = &test_variable;
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), ERROR_DS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), ERROR_DS_MEMORY_OUT_OF_DS);
    ds_deinit_allocation();
   }
 */
TEST(Free_Tests, Free_Ptr_Is_Null)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, (void **)&pointer, allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, (void **)&pointer), ERROR_DS_OK);
    ASSERT_TRUE(pointer == NULL);
    ds_deinit_allocation(&ds_buffer);
}
TEST(Free_Tests, Free_Twice)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, (void **)&pointer, allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, (void **)&pointer), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, (void **)&pointer), ERROR_DS_INVALID_ARG);
    ds_deinit_allocation(&ds_buffer);
}
/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Free_Memory_Unlocked)
   {
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t memory_usage = 0;
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer, allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free((void **)&pointer), ERROR_DS_OK);
    ASSERT_EQ(ds_get_memory_usage(&memory_usage), ERROR_DS_OK);
    GOUT.info() << " Read memory usage: " << ((unsigned int) memory_usage) << std::endl;
    ASSERT_EQ(memory_usage, 0);
    ds_deinit_allocation();
   }
 */
