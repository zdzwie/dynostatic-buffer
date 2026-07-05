/**
 * @file utests-malloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for malloc function behaviour.
 * @version 0.2
 * @date 2026-07-03
 */
#include "utests-common.hpp"
#include "logger.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::ExpectedUsage;

constexpr uint8_t max_memory_usage_deviation = 2; /**< Maximal deviation between calculated and read memory usage. */

class Malloc_Tests : public DsBufferTest {};

TEST(Malloc_NoFixture_Tests, Malloc_UnInitialized)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&pointer), 5), ERROR_DS_NO_INIT);
}

TEST_F(Malloc_Tests, Malloc_Bad_Input_Params)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&buf_, NULL, 5), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 0), ERROR_DS_INVALID_ARG);
}

TEST_F(Malloc_Tests, Malloc_To_Big_Chunk)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), DS_MAX_ALLOCATION_SIZE + 1),
              ERROR_DS_TOO_BIG_CHUNK);
}

TEST_F(Malloc_Tests, Malloc_No_NULL)
{
    char *pointer = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 5), ERROR_DS_OK);
    ASSERT_TRUE(pointer != NULL);
}

TEST_F(Malloc_Tests, Malloc_Too_Many_Times)
{
    char *pointer = NULL;

    /* Premise guard: allocator slots must run out BEFORE memory does,
     * otherwise this test would fail with ERROR_DS_NO_MEMORY instead. */
    ASSERT_LE(DS_MAX_ALLOCATION_COUNT * AlignUp(1u), DS_BUFFER_MEMORY_SIZE);

    for (unsigned int iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 1), ERROR_DS_OK) << "iteration " << iter;
        pointer = NULL;
    }

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), 1), ERROR_DS_NO_ALLOCATORS);
}

TEST_F(Malloc_Tests, Malloc_Lack_Of_Memory)
{
    char *pointer = NULL;
    const size_t chunk = AlignUp(DS_MAX_ALLOCATION_SIZE);
    const size_t full_chunks = DS_BUFFER_MEMORY_SIZE / chunk;

    /* Premise guard: memory must run out BEFORE allocator slots do. */
    ASSERT_LT(full_chunks, DS_MAX_ALLOCATION_COUNT);

    for (size_t iter = 0; iter < full_chunks; iter++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK)
            << "iteration " << iter;
        pointer = NULL;
    }

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), DS_MAX_ALLOCATION_SIZE), ERROR_DS_NO_MEMORY);
}

TEST_F(Malloc_Tests, Malloc_Proper_Size)
{
    char *pointer = NULL;
    uint8_t r_memory_usage = 0;
    const uint8_t c_memory_usage = ExpectedUsage(AlignUp(DS_MAX_ALLOCATION_SIZE));

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer), DS_MAX_ALLOCATION_SIZE), ERROR_DS_OK);
    ASSERT_EQ(ds_get_memory_usage(&buf_, &r_memory_usage), ERROR_DS_OK);

    Logger::info("Calculated usage: ", (unsigned int)c_memory_usage,
                 " Read memory usage: ", (unsigned int)r_memory_usage);

    ASSERT_NEAR(r_memory_usage, c_memory_usage, max_memory_usage_deviation);
}

TEST_F(Malloc_Tests, Malloc_Proper_Allocators)
{
    char *first_pointer = NULL;
    char *second_pointer = NULL;
    const size_t allocation_len = 10;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&first_pointer), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&second_pointer), allocation_len), ERROR_DS_OK);

    const size_t ptr_diff = (size_t)(second_pointer - first_pointer);

    Logger::info("Distance in memory of two pointers: ", (unsigned int)ptr_diff, " bytes");

    /* Physical stride is the ALIGNED size; with the DS_ALIGNMENT == 1
     * fallback this degrades to the raw length (pre-alignment behaviour). */
    ASSERT_EQ(ptr_diff, AlignUp(allocation_len));
}

/* Behavioural catcher for the wrong-pointer-return bug: patterns written to
 * two adjacent blocks must not clobber each other, and each block must be
 * readable back in full. */
TEST_F(Malloc_Tests, Blocks_Do_Not_Overlap)
{
    uint8_t *p1 = NULL;
    uint8_t *p2 = NULL;
    const size_t len = 16;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), len), ERROR_DS_OK);

    std::memset(p1, 0xA5, len);
    std::memset(p2, 0x5A, len);

    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p1[i], 0xA5) << "block 1 clobbered at byte " << i;
        ASSERT_EQ(p2[i], 0x5A) << "block 2 clobbered at byte " << i;
    }
}
