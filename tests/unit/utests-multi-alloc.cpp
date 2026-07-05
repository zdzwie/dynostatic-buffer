/**
 * @file utests-multi-alloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Unit tests for multi-allocation behaviour.
 * @version 0.2
 * @date 2026-07-03
 */
#include "utests-common.hpp"
#include "logger.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::ExpectedUsage;

constexpr uint8_t max_memory_usage_deviation = 2; /**< Maximal deviation between calculated and read memory usage. */

class Multi_Alloc_Tests : public DsBufferTest {};

TEST_F(Multi_Alloc_Tests, Malloc_Few_Times)
{
    char *pointer1 = NULL;
    char *pointer2 = NULL;
    const size_t allocation_len = DS_MAX_ALLOCATION_SIZE;
    uint8_t r_usage = 0;

    ASSERT_LE(2u * AlignUp(allocation_len), static_cast<size_t>(DS_BUFFER_MEMORY_SIZE));

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer1), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer2), allocation_len), ERROR_DS_OK);

    /* size_t arithmetic end-to-end — the old uint16_t cast silently
     * truncated for larger buffer configurations. */
    const uint8_t c_usage = ExpectedUsage(2u * AlignUp(allocation_len));

    ASSERT_EQ(ds_get_memory_usage(&buf_, &r_usage), ERROR_DS_OK);

    Logger::info("Calculated usage: ", (unsigned int)c_usage,
                 " Read memory usage: ", (unsigned int)r_usage);

    ASSERT_NEAR(r_usage, c_usage, max_memory_usage_deviation);
}

/* Interleaved pattern integrity across several allocations — behavioural
 * catcher for wrong-pointer-return and head-accounting bugs at once. */
TEST_F(Multi_Alloc_Tests, Interleaved_Blocks_Keep_Contents)
{
    constexpr size_t kBlocks = 4;
    constexpr size_t kLen = 8;
    uint8_t *blocks[kBlocks] = { NULL };

    for (size_t b = 0; b < kBlocks; b++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&blocks[b]), kLen), ERROR_DS_OK);
        std::memset(blocks[b], static_cast<int>(0x10u * (b + 1u)), kLen);
    }

    for (size_t b = 0; b < kBlocks; b++) {
        for (size_t i = 0; i < kLen; i++) {
            ASSERT_EQ(blocks[b][i], static_cast<uint8_t>(0x10u * (b + 1u)))
                << "block " << b << " byte " << i << " clobbered";
        }
    }
}

/* TODO: Fix it and uncomment that test.
   TEST_F(Multi_Alloc_Tests, Malloc_Twice_Non_Free_Ptr)
   {
    char *pointer1 = NULL;
    const size_t allocation_len = DS_MAX_ALLOCATION_SIZE;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer1), allocation_len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&pointer1), allocation_len), ERROR_DS_PTR_ALLOC_YET);
   }
 */
