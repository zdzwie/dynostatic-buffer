/**
 * @file utests-calloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for calloc function behaviour.
 * @version 0.2
 * @date 2026-07-03
 */
#include "utests-common.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::ExpectedUsage;

class Calloc_Tests : public DsBufferTest {};

TEST(Calloc_NoFixture_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;

    ASSERT_EQ(ds_calloc(&ds_buffer, reinterpret_cast<void **>(&p), 4, 4), ERROR_DS_NO_INIT);
}

TEST(Calloc_NoFixture_Tests, Null_Buffer)
{
    char *p = NULL;

    ASSERT_EQ(ds_calloc(NULL, reinterpret_cast<void **>(&p), 4, 4), ERROR_DS_INVALID_ARG);
}

TEST_F(Calloc_Tests, Bad_Args)
{
    char *p = NULL;

    ASSERT_EQ(ds_calloc(&buf_, NULL, 4, 4), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), 0, 4), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), 4, 0), ERROR_DS_INVALID_ARG);
}

TEST_F(Calloc_Tests, Overflow_Is_Rejected)
{
    char *p = NULL;
    const size_t huge = (SIZE_MAX / 2) + 1; /* huge * 4 overflows size_t */

    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), huge, 4), ERROR_DS_INVALID_ARG);
}

TEST_F(Calloc_Tests, Total_Size_Too_Big)
{
    char *p = NULL;

    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), DS_MAX_ALLOCATION_SIZE, 2),
              ERROR_DS_TOO_BIG_CHUNK);
}

TEST_F(Calloc_Tests, Memory_Is_Zeroed)
{
    uint8_t *p = NULL;
    const size_t len = 8;
    const size_t elem = 4;

    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), len, elem), ERROR_DS_OK);
    ASSERT_TRUE(p != NULL);

    for (size_t i = 0; i < len * elem; i++) {
        ASSERT_EQ(p[i], 0u) << "byte " << i << " not zeroed";
    }
}

TEST_F(Calloc_Tests, Usage_Reflects_Total_Size)
{
    uint8_t *p = NULL;
    const size_t len = 8;
    const size_t elem = 4;
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), len, elem), ERROR_DS_OK);
    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, ExpectedUsage(AlignUp(len * elem)));
}

/* TODO: Fix/implement in DS-Buffer and uncomment (needs working ds_free with
 * block reuse). A REUSED block is dirty by definition — calloc must zero it.
 * This is the regression trap for the ds_zero(dest_size) bug in ds_calloc.
   TEST_F(Calloc_Tests, Reused_Block_Is_Zeroed)
   {
    uint8_t *p = NULL;
    char *guard = NULL;
    const size_t len = 16;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&guard), 8), ERROR_DS_OK);

    std::memset(p, 0xFF, len); // dirty the block before freeing
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK);

    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&p), len, 1), ERROR_DS_OK);
    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p[i], 0u) << "reused byte " << i << " not zeroed";
    }

    // The guard block must be untouched by calloc's zeroing.
    // (guard was never written, so verify via a written pattern instead:)
   }
 */

TEST_F(Calloc_Tests, Zeroing_Does_Not_Spill_Into_Neighbour)
{
    uint8_t *first = NULL;
    uint8_t *zeroed = NULL;
    const size_t len = 16;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&first), len), ERROR_DS_OK);
    std::memset(first, 0xEE, len);

    ASSERT_EQ(ds_calloc(&buf_, reinterpret_cast<void **>(&zeroed), len, 1), ERROR_DS_OK);

    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(first[i], 0xEE) << "neighbour byte " << i << " clobbered by calloc zeroing";
        ASSERT_EQ(zeroed[i], 0u) << "calloc byte " << i << " not zeroed";
    }
}
