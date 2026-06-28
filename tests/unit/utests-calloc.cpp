/**
 * @file utests-calloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for calloc function behaviour.
 * @version 0.1
 * @date 2026-06-28
 *
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstddef>
#include <cstring>

extern "C" {
#include "dynostatic-buffer.h"
#include "error.h"
}

TEST(Calloc_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;
    ASSERT_EQ(ds_calloc(&ds_buffer, reinterpret_cast<void **>(&p), 4, 4), ERROR_DS_NO_INIT);
}

TEST(Calloc_Tests, Memory_Is_Zeroed)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t *p = NULL;
    const size_t len = 8;
    const size_t elem = 4;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_calloc(&ds_buffer, reinterpret_cast<void **>(&p), len, elem), ERROR_DS_OK);
    ASSERT_TRUE(p != NULL);

    for (size_t i = 0; i < len * elem; i++) {
        ASSERT_EQ(p[i], 0u) << "byte " << i << " not zeroed";
    }

    ds_deinit_allocation(&ds_buffer);
}

TEST(Calloc_Tests, Overflow_Is_Rejected)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;
    const size_t huge = (SIZE_MAX / 2) + 1; /* huge * 4 overflows size_t */

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_calloc(&ds_buffer, reinterpret_cast<void **>(&p), huge, 4), ERROR_DS_INVALID_ARG);

    ds_deinit_allocation(&ds_buffer);
}

TEST(Calloc_Tests, Bad_Args)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_calloc(&ds_buffer, NULL, 4, 4), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_calloc(&ds_buffer, reinterpret_cast<void **>(&p), 0, 4), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_calloc(&ds_buffer, reinterpret_cast<void **>(&p), 4, 0), ERROR_DS_INVALID_ARG);

    ds_deinit_allocation(&ds_buffer);
}
