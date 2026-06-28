/**
 * @file utests-alligment.cpp
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

TEST(Realloc_Tests, Null_Acts_As_Malloc)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    void *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 16), ERROR_DS_OK);
    ASSERT_TRUE(p != NULL);

    ds_deinit_allocation(&ds_buffer);
}

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Realloc_Tests, Zero_Size_Acts_As_Free)
   {
    dynostatic_buffer_t ds_buffer = { 0 };
    void *p = NULL;
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 32), ERROR_DS_OK);
    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 0), ERROR_DS_OK);
    ASSERT_TRUE(p == NULL);

    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0);

    ds_deinit_allocation(&ds_buffer);
   }
 */

TEST(Realloc_Tests, Shrink_Keeps_Pointer)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    void *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 64), ERROR_DS_OK);
    void *before = p;
    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 16), ERROR_DS_OK);
    ASSERT_EQ(p, before); /* shrink in place */

    ds_deinit_allocation(&ds_buffer);
}

TEST(Realloc_Tests, Grow_Preserves_Contents)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    void *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 10), ERROR_DS_OK);

    uint8_t *bytes = static_cast<uint8_t *>(p);
    for (uint8_t i = 0; i < 10; i++) {
        bytes[i] = static_cast<uint8_t>(i + 1);
    }

    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 40), ERROR_DS_OK);

    bytes = static_cast<uint8_t *>(p);
    for (uint8_t i = 0; i < 10; i++) {
        ASSERT_EQ(bytes[i], static_cast<uint8_t>(i + 1)) << "byte " << (int)i << " not preserved";
    }

    ds_deinit_allocation(&ds_buffer);
}
