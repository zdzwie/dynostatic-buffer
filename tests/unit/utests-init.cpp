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

#include "utests-common.hpp"

#include "dynostatic-buffer.h"
#include "ds-defs.h"

TEST(Initialization_Tests, Twice_Initialize)
{
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_ALREADY_INIT);

    ds_deinit_allocation();
}

TEST(Initialization_Tests, Deinit)
{
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);

    ds_deinit_allocation();
    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);

    ds_deinit_allocation();
}

TEST(Initialization_Tests, Bad_Logger)
{
    ASSERT_EQ(ds_initialize_allocation(NULL), EDS_INVALID_PARAMS);
    ds_deinit_allocation();
}
