/**
 * @file utests-init.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for initialization of dynostatic buffer.
 * @version 0.1
 * @date 2022-07-14
 *
 */
#include <gtest/gtest.h>

extern "C" {
#include "dynostatic-buffer.h"
#include "error.h"
}

TEST(Initialization_Tests, Twice_Initialize)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_ALREADY_INIT);
    ds_deinit_allocation(&ds_buffer);
}

TEST(Initialization_Tests, Deinit)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ds_deinit_allocation(&ds_buffer);
    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ds_deinit_allocation(&ds_buffer);
}
TEST(Initialization_Tests, invalid_args)
{
    ASSERT_EQ(ds_initialize_allocation(NULL), ERROR_DS_INVALID_ARG);
}
