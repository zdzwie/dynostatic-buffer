/**
 * @file utests-alligment.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for alignment behaviour.
 * @version 0.3
 * @date 2026-07-04
 */
#include "utests-common.hpp"


using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::IsAligned;

class Alignment_Tests : public DsBufferTest {};

class Alignment_Param_Tests : public Alignment_Tests,
                              public ::testing::WithParamInterface<size_t> {};

TEST_P(Alignment_Param_Tests, Malloc_Returns_Aligned_Pointer)
{
    void *p = nullptr;

    ASSERT_EQ(ds_malloc(&buf_, &p, GetParam()), ERROR_DS_OK);
    ASSERT_TRUE(IsAligned(p)) << "size=" << GetParam();
}

TEST_P(Alignment_Param_Tests, Calloc_Returns_Aligned_Pointer)
{
    void *p = nullptr;

    ASSERT_EQ(ds_calloc(&buf_, &p, GetParam(), 1u), ERROR_DS_OK);
    ASSERT_TRUE(IsAligned(p)) << "len=" << GetParam();
}

INSTANTIATE_TEST_SUITE_P(
    Various_Sizes, Alignment_Param_Tests,
    ::testing::Values(1u, 2u, 3u, static_cast<size_t>(DS_ALIGNMENT),
                      static_cast<size_t>(DS_ALIGNMENT) + 1u,
                      (2u * DS_ALIGNMENT) - 1u, 2u * DS_ALIGNMENT));

TEST_F(Alignment_Tests, Consecutive_Stride_Is_Aligned)
{
    char *p1 = NULL;
    char *p2 = NULL;
    const size_t len = 10; /* not a multiple of DS_ALIGNMENT (for align >= 4) */

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), len), ERROR_DS_OK);

    /* Second block starts at the aligned end of the first, not raw len bytes. */
    ASSERT_EQ(static_cast<size_t>(p2 - p1), AlignUp(len));
    ASSERT_TRUE(IsAligned(p2));
}

TEST_F(Alignment_Tests, Minimal_Requests_Consume_Full_Alignment)
{
    char *p1 = NULL;
    char *p2 = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), 1), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), 1), ERROR_DS_OK);

    ASSERT_EQ(static_cast<size_t>(p2 - p1), static_cast<size_t>(DS_ALIGNMENT))
        << "malloc(1) must physically consume DS_ALIGNMENT bytes";
}

/* Boundary: aligned-size accounting at the end of the buffer. A raw request
 * would fit in the remaining space, but its aligned size does not. */
TEST_F(Alignment_Tests, Aligned_Size_Exceeds_Remaining_Space)
{
    /* Consume the buffer down to exactly one aligned slot. */
    size_t remaining = DS_BUFFER_MEMORY_SIZE - (DS_BUFFER_MEMORY_SIZE % DS_ALIGNMENT);
    char *p = NULL;

    ASSERT_EQ(AlignUp(DS_MAX_ALLOCATION_SIZE), static_cast<size_t>(DS_MAX_ALLOCATION_SIZE))
        << "test premise: DS_MAX_ALLOCATION_SIZE must be alignment-multiple";

    while (remaining > DS_ALIGNMENT) {
        size_t chunk = remaining - DS_ALIGNMENT;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), chunk), ERROR_DS_OK);
        p = NULL;
        remaining -= chunk;
    }

    /* Exactly DS_ALIGNMENT bytes of aligned space remain: a request of
     * DS_ALIGNMENT + 1 rounds to 2*DS_ALIGNMENT and must be rejected... */
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), DS_ALIGNMENT + 1), ERROR_DS_NO_MEMORY);

    /* ...while an unaligned request of 1 byte still fits the last slot. */
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1), ERROR_DS_OK);
    ASSERT_TRUE(IsAligned(p));

    /* Buffer is now physically full despite the last request being 1 byte. */
    p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1), ERROR_DS_NO_MEMORY);
}

/* TODO: Fix/implement in DS-Buffer and uncomment (needs working ds_free with
 * block reuse — the guard pins the bump head, forcing the reuse path).
   TEST_F(Alignment_Tests, Reused_Block_Is_Aligned)
   {
    char *p1 = NULL;
    char *p2 = NULL;
    char *guard = NULL;

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), 3), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&guard), 1), ERROR_DS_OK);

    char *original = p1;
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p1)), ERROR_DS_OK);

    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), 1), ERROR_DS_OK);
    ASSERT_TRUE(IsAligned(p2));
    ASSERT_EQ(p2, original);
   }
 */
