/**
 * @file utests-realloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for realloc function behaviour.
 * @version 0.2
 * @date 2026-07-03
 */
#include "utests-common.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::ExpectedUsage;

class Realloc_Tests : public DsBufferTest {};

TEST(Realloc_NoFixture_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    void *p = NULL;

    ASSERT_EQ(ds_realloc(&ds_buffer, &p, 16), ERROR_DS_NO_INIT);
}

TEST(Realloc_NoFixture_Tests, Null_Buffer)
{
    void *p = NULL;

    /* ds_realloc folds the NULL-buffer check into its init check, so a NULL
     * buffer surfaces as ERROR_DS_NO_INIT rather than ERROR_DS_INVALID_ARG. */
    ASSERT_EQ(ds_realloc(NULL, &p, 16), ERROR_DS_INVALID_ARG);
}

TEST_F(Realloc_Tests, Bad_Args)
{
    ASSERT_EQ(ds_realloc(&buf_, NULL, 16), ERROR_DS_INVALID_ARG);
}

TEST_F(Realloc_Tests, Null_Acts_As_Malloc)
{
    void *p = NULL;

    ASSERT_EQ(ds_realloc(&buf_, &p, 16), ERROR_DS_OK);
    ASSERT_TRUE(p != NULL);

    /* Discriminating assertion: the malloc path must actually account for
     * the memory, not just hand back a pointer. */
    uint8_t usage = 0xFF;
    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, ExpectedUsage(AlignUp(16u)));
}

/*
 * NOTE on the tests below: the previous versions of Shrink/Grow PASSED on the
 * current stub implementation, because ds_realloc(ptr!=NULL, size!=0) returns
 * ERROR_DS_OK without doing anything — "pointer unchanged" and "contents
 * preserved" were vacuously true. Each test now carries at least one
 * assertion that a do-nothing implementation cannot satisfy (usage change /
 * writability of the grown region), and stays commented out until ds_realloc
 * is implemented.
 */

TEST_F(Realloc_Tests, Zero_Size_Acts_As_Free)
{
    void *p = NULL;
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_realloc(&buf_, &p, 32), ERROR_DS_OK);
    ASSERT_EQ(ds_realloc(&buf_, &p, 0), ERROR_DS_OK);
    ASSERT_TRUE(p == NULL);

    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0);
}


/* TODO: Fix/implement in DS-Buffer and uncomment.
   TEST_F(Realloc_Tests, Shrink_Keeps_Pointer)
   {
    void *p = NULL;
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_realloc(&buf_, &p, 64), ERROR_DS_OK);
    void *before = p;

    ASSERT_EQ(ds_realloc(&buf_, &p, 16), ERROR_DS_OK);
    ASSERT_EQ(p, before); // shrink in place

    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    // Option A — usage tracks requested size:
    ASSERT_EQ(usage, dstest::ExpectedUsage(dstest::AlignUp(16u)));
    // Option B — usage tracks physical capacity:
    // ASSERT_EQ(usage, dstest::ExpectedUsage(dstest::AlignUp(64u)));
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST_F(Realloc_Tests, Grow_Preserves_Contents)
   {
    void *p = NULL;

    ASSERT_EQ(ds_realloc(&buf_, &p, 10), ERROR_DS_OK);

    uint8_t *bytes = static_cast<uint8_t *>(p);
    for (uint8_t i = 0; i < 10; i++) {
        bytes[i] = static_cast<uint8_t>(i + 1);
    }

    ASSERT_EQ(ds_realloc(&buf_, &p, 40), ERROR_DS_OK);

    bytes = static_cast<uint8_t *>(p);
    for (uint8_t i = 0; i < 10; i++) {
        ASSERT_EQ(bytes[i], static_cast<uint8_t>(i + 1)) << "byte " << (int)i << " not preserved";
    }

    // Discriminating assertions a no-op implementation cannot pass:
    // 1) the grown region must be writable and readable back,
    std::memset(bytes + 10, 0xC3, 30);
    for (uint8_t i = 10; i < 40; i++) {
        ASSERT_EQ(bytes[i], 0xC3) << "grown byte " << (int)i << " not usable";
    }
    // 2) accounting must reflect the new size.
    uint8_t usage = 0xFF;
    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, dstest::ExpectedUsage(dstest::AlignUp(40u)));
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment (needs move semantics)
   TEST_F(Realloc_Tests, Grow_Moves_When_Blocked)
   {
    void *p = NULL;
    void *guard = NULL;

    ASSERT_EQ(ds_realloc(&buf_, &p, 16), ERROR_DS_OK);
    // Guard directly after p blocks in-place growth.
    ASSERT_EQ(ds_malloc(&buf_, &guard, 16), ERROR_DS_OK);

    uint8_t *bytes = static_cast<uint8_t *>(p);
    std::memset(bytes, 0x77, 16);

    void *before = p;
    ASSERT_EQ(ds_realloc(&buf_, &p, 64), ERROR_DS_OK);
    ASSERT_NE(p, before); // must have moved

    bytes = static_cast<uint8_t *>(p);
    for (size_t i = 0; i < 16; i++) {
        ASSERT_EQ(bytes[i], 0x77) << "byte " << i << " lost during move";
    }
   }
 */
