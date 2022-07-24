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

#include "gtest_out.hpp"

#include "utests-common.hpp"

#include "dynostatic-buffer.h"
#include "ds-defs.h"

TEST(Multi_Alloc_Tests, Malloc_Few_Times)
{
    char *pointer1 = NULL;
    char *pointer2 = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t c_usage = 0;
    uint8_t r_usage = 0;

    ASSERT_TRUE((2 * DS_MAX_ALLOCATION_SIZE) <= DS_BUFFER_MEMORY_SIZE);

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer1, allocation_len), EDS_OK);

    c_usage = (uint8_t) ((((uint16_t )(2ul * allocation_len)) * 100ul) / DS_BUFFER_MEMORY_SIZE);
    ASSERT_EQ(ds_malloc((void **)&pointer2, allocation_len), EDS_OK);

    ASSERT_EQ(ds_get_memory_usage(&r_usage), EDS_OK);

    GOUT.info() << "Calculated usage: " << ((unsigned int) c_usage) << " Read memory usage: " << ((unsigned int) r_usage) << std::endl;

    ASSERT_TRUE(abs(r_usage - c_usage) < max_memory_usage_dev);
    ds_deinit_allocation();
}

TEST(Multi_Alloc_Tests, Malloc_Twice_Non_Free_Ptr)
{
    char *pointer1 = NULL;
    size_t allocation_len = DS_MAX_ALLOCATION_SIZE;

    ASSERT_EQ(ds_initialize_allocation(utests_stdout_logger), EDS_OK);
    ASSERT_EQ(ds_malloc((void **)&pointer1, allocation_len), EDS_OK);

    ASSERT_EQ(ds_malloc((void **)&pointer1, allocation_len), EDS_PTR_ALLOC_YET);
    ds_deinit_allocation();
}
