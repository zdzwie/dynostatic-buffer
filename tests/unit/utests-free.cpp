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
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_NO_INIT);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
    ds_deinit_allocation(&ds_buffer);
}
TEST(Free_Tests, Free_Bad_Input_Params)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_INVALID_ARG);
    ds_deinit_allocation(&ds_buffer);
}
/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Free_Pointer_Outside_DS)
   {
    char test_variable = 5;
    char *pointer = &test_variable;
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), ERROR_DS_OK);
    ASSERT_EQ(ds_free(reinterpret_cast<void **>(&pointer)), ERROR_DS_MEMORY_OUT_OF_DS);
    ds_deinit_allocation();
   }
 */
TEST(Free_Tests, Free_Ptr_Is_Null)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
    ASSERT_TRUE(pointer == NULL);
    ds_deinit_allocation(&ds_buffer);
}
TEST(Free_Tests, Free_Twice)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;
    size_t allocation_len = 5;
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&pointer)), ERROR_DS_INVALID_ARG);
    ds_deinit_allocation(&ds_buffer);
}
/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Free_Memory_Unlocked)
   {
    char *pointer = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t memory_usage = 0;
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(reinterpret_cast<void **>(&pointer), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_free(reinterpret_cast<void **>(&pointer)), ERROR_DS_OK);
    ASSERT_EQ(ds_get_memory_usage(&memory_usage), ERROR_DS_OK);
    GOUT.info() << " Read memory usage: " << ((unsigned int) memory_usage) << std::endl;
    ASSERT_EQ(memory_usage, 0);
    ds_deinit_allocation();
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Free_Releases_Memory)
   {
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&p)), ERROR_DS_OK);

    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0); // no live ALLOCATED descriptors -> 0%

    ds_deinit_allocation(&ds_buffer);
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Freed_Slot_Is_Reused)
   {
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p1 = NULL;
    char *p2 = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p1), 50), ERROR_DS_OK);

    char *freed_addr = p1;
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&p1)), ERROR_DS_OK);

    // New, smaller request must reuse the just-freed block (same address).
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p2), 40), ERROR_DS_OK);
    ASSERT_EQ(p2, freed_addr);

    ds_deinit_allocation(&ds_buffer);
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Last_Block_Reclaims_Bump_Head)
   {
    // Optional Phase-2 bonus: freeing the most-recent block rolls data_head back.
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p1 = NULL;
    char *p2 = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p1), 100), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p2), 100), ERROR_DS_OK);

    size_t head_before = ds_buffer.data_head;
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&p2)), ERROR_DS_OK);
    ASSERT_LT(ds_buffer.data_head, head_before);

    ds_deinit_allocation(&ds_buffer);
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Free_Tests, Real_Double_Free_Is_Rejected)
   {
    // Unlike Free_Twice, keep an alias so the second call sees a non-NULL,
    // already-freed address — this exercises the real double-free path.
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p), 5), ERROR_DS_OK);

    char *alias = p;
    ASSERT_EQ(ds_free(&ds_buffer, reinterpret_cast<void **>(&p)), ERROR_DS_OK);

    // Pick whatever code you settle on (MEMORY_OUT_OF_DS / a dedicated one);
    //   the contract is simply: a double free must NOT succeed.
    ASSERT_NE(ds_free(&ds_buffer, reinterpret_cast<void **>(&alias)), ERROR_DS_OK);

    ds_deinit_allocation(&ds_buffer);
   }
 */
