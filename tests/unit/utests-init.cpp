/**
 * @file utests-init.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for initialization and deinitialization of dynostatic buffer.
 * @version 0.2
 * @date 2026-07-03
 */
#include "utests-common.hpp"

TEST(Initialization_Tests, Invalid_Args)
{
    ASSERT_EQ(ds_initialize_allocation(NULL), ERROR_DS_INVALID_ARG);
}

TEST(Initialization_Tests, Twice_Initialize)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_ALREADY_INIT);
    ds_deinit_allocation(&ds_buffer);
}

TEST(Initialization_Tests, Deinit_Then_Reinit)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_deinit_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_deinit_allocation(&ds_buffer), ERROR_DS_OK);
}

TEST(Initialization_Tests, Deinit_Without_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    ASSERT_EQ(ds_deinit_allocation(&ds_buffer), ERROR_DS_NO_INIT);
}

TEST(Initialization_Tests, Deinit_Null_Buffer)
{
    ASSERT_EQ(ds_deinit_allocation(NULL), ERROR_DS_INVALID_ARG);
}

/* Behavioural check that a full deinit/init cycle really resets allocator
 * state: after the cycle the whole buffer must be allocatable again. */
TEST(Initialization_Tests, Reinit_Restores_Full_Capacity)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);

    ASSERT_EQ(ds_deinit_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);

    uint8_t usage = 0xFF;
    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0u);

    p = NULL;
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);

    ds_deinit_allocation(&ds_buffer);
}

/* TODO: Fix/implement in DS-Buffer and uncomment (implementation bug: init
 * does not reset data_head/used_allocators, deinit does — init must be
 * self-sufficient for structs with garbage content).
 * NOTE: deliberate white-box test — it simulates a non-zeroed struct, which
 * is unreachable through the public API alone.
   TEST(Initialization_Tests, Init_Resets_Garbage_State)
   {
    dynostatic_buffer_t ds_buffer;
    std::memset(&ds_buffer, 0xAA, sizeof(ds_buffer));
    ds_buffer.initialized = false;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);

    char *p = NULL;
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);

    ds_deinit_allocation(&ds_buffer);
   }
 */
