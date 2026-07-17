/**
 * @file utests-safe-memory-copy.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_safe_memory_copy — bounded copy into live blocks.
 * @version 0.1
 * @date 2026-07-12
 */
#include "utests-common.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;

namespace {
constexpr uint8_t kPrefill = 0xEEu;
constexpr uint8_t kPattern = 0x5Au;
} // namespace

class Safe_Memory_Copy_Tests : public DsBufferTest {};

/*--------------------- Validation paths ---------------------*/

TEST(Safe_Memory_Copy_NoFixture_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t src[4] = { 0 };
    uint8_t dst_dummy = 0;
    ASSERT_EQ(ds_safe_memory_copy(&ds_buffer, &dst_dummy, src, sizeof(src)), ERROR_DS_NO_INIT);
}

TEST_F(Safe_Memory_Copy_Tests, Bad_Input_Params)
{
    uint8_t src[4] = { 0 };
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), AlignUp(8u)), ERROR_DS_OK);

    ASSERT_EQ(ds_safe_memory_copy(NULL, p, src, sizeof(src)), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_safe_memory_copy(&buf_, NULL, src, sizeof(src)), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_safe_memory_copy(&buf_, p, NULL, sizeof(src)), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_safe_memory_copy(&buf_, p, src, 0u), ERROR_DS_INVALID_ARG);
}

/*--------------------- Happy paths ---------------------*/

TEST_F(Safe_Memory_Copy_Tests, Copies_Into_Block_Start)
{
    const size_t len = AlignUp(16u); /* aligned request -> capacity == len */
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);

    uint8_t src[8];
    std::memset(src, kPattern, sizeof(src));

    ASSERT_EQ(ds_safe_memory_copy(&buf_, p, src, sizeof(src)), ERROR_DS_OK);
    for (size_t i = 0; i < sizeof(src); i++) {
        ASSERT_EQ(p[i], kPattern) << "byte " << i;
    }
}

/* Variant-B discriminator: an INTERIOR destination address must work, and
 * the copy must land exactly at that offset — bytes on both sides intact.
 * Offset 3 is deliberately unaligned: interior addresses carry no
 * alignment guarantee. */
TEST_F(Safe_Memory_Copy_Tests, Copies_Into_Block_Interior)
{
    const size_t len = AlignUp(16u);
    const size_t shift = 3u;
    const size_t copy_len = 4u;
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    uint8_t src[copy_len];
    std::memset(src, kPattern, sizeof(src));

    ASSERT_EQ(ds_safe_memory_copy(&buf_, p + shift, src, copy_len), ERROR_DS_OK);

    for (size_t i = 0; i < shift; i++) {
        ASSERT_EQ(p[i], kPrefill) << "prefix byte " << i << " clobbered";
    }
    for (size_t i = shift; i < shift + copy_len; i++) {
        ASSERT_EQ(p[i], kPattern) << "copied byte " << i;
    }
    for (size_t i = shift + copy_len; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "suffix byte " << i << " clobbered";
    }
}

TEST_F(Safe_Memory_Copy_Tests, Exact_Fit_To_Block_End)
{
    const size_t len = AlignUp(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);

    uint8_t src[AlignUp(16u)];
    std::memset(src, kPattern, sizeof(src));

    /* shift + (len - shift) == capacity: the boundary case that must PASS. */
    ASSERT_EQ(ds_safe_memory_copy(&buf_, p + shift, src, len - shift), ERROR_DS_OK);
    ASSERT_EQ(p[len - 1u], kPattern) << "last byte of the block not written";
}

/*--------------------- Bounds violations ---------------------*/

TEST_F(Safe_Memory_Copy_Tests, Rejects_One_Byte_Past_End)
{
    const size_t len = AlignUp(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    uint8_t src[AlignUp(16u)];
    std::memset(src, kPattern, sizeof(src));

    ASSERT_EQ(ds_safe_memory_copy(&buf_, p + shift, src, (len - shift) + 1u), ERROR_DS_NO_MEMORY);

    /* Rejection must be side-effect free. */
    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "byte " << i << " modified by a rejected call";
    }
}

/* Overflow guard: shift + src_size wrapping around SIZE_MAX must not slip
 * past the bounds check. A naive `shift + size > capacity` check PASSES
 * these inputs — this test exists to keep the overflow-safe form in place. */
TEST_F(Safe_Memory_Copy_Tests, Rejects_Wrapping_Size)
{
    const size_t len = AlignUp(16u);
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    uint8_t src[1] = { kPattern }; /* never read: the copy must not execute */

    ASSERT_EQ(ds_safe_memory_copy(&buf_, p, src, SIZE_MAX), ERROR_DS_NO_MEMORY);
    ASSERT_EQ(ds_safe_memory_copy(&buf_, p + 3, src, SIZE_MAX - 2u), ERROR_DS_NO_MEMORY);

    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "byte " << i << " modified by a rejected call";
    }
}

/*--------------------- Lookup outcome map ---------------------*/

TEST_F(Safe_Memory_Copy_Tests, Rejects_Foreign_Pointer)
{
    uint8_t src[4] = { 0 };
    uint8_t stack_var = 0;
    ASSERT_EQ(ds_safe_memory_copy(&buf_, &stack_var, src, sizeof(src)), ERROR_DS_MEMORY_OUT_OF_DS);
}

TEST_F(Safe_Memory_Copy_Tests, Rejects_Pointer_In_Parked_Block)
{
    const size_t len = AlignUp(16u);
    uint8_t *p = nullptr;
    uint8_t *guard = nullptr;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&guard), 1u), ERROR_DS_OK); /* pins the head */

    uint8_t *alias = p;
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK); /* parked DS_FREE */

    uint8_t src[4] = { 0 };
    ASSERT_EQ(ds_safe_memory_copy(&buf_, alias, src, sizeof(src)), ERROR_DS_ALLOCATOR_NOT_FOUND);
    ASSERT_EQ(ds_safe_memory_copy(&buf_, alias + 2, src, sizeof(src)), ERROR_DS_ALLOCATOR_NOT_FOUND);
}

/* Reclaimed space with a PARTIAL prefix: the containing-scan exits via the
 * break on the first DS_NOT_USED record. */
TEST_F(Safe_Memory_Copy_Tests, Rejects_Reclaimed_Space)
{
    const size_t len = AlignUp(16u);
    uint8_t *p = nullptr;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);

    uint8_t *alias = p;
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK); /* trailing -> rollback */

    uint8_t src[4] = { 0 };
    ASSERT_EQ(ds_safe_memory_copy(&buf_, alias, src, sizeof(src)), ERROR_DS_ALLOCATOR_NOT_FOUND);
}

/* Past data_head with a FULL prefix: no DS_NOT_USED record exists, so the
 * containing-scan must exit through its natural loop end — the branch
 * that is easiest to leave untested. */
TEST_F(Safe_Memory_Copy_Tests, Rejects_Past_Head_With_Full_Prefix)
{
    ASSERT_LE(static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) * AlignUp(1u) + AlignUp(4u),
              static_cast<size_t>(DS_BUFFER_MEMORY_SIZE))
        << "premise: arena space must remain after slot exhaustion";

    uint8_t *p = nullptr;
    uint8_t *last = nullptr;
    for (size_t iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1u), ERROR_DS_OK);
        last = p;
        p = nullptr;
    }

    if (nullptr == last) {
        FAIL() << "premise: at least one allocation must have succeeded";
    }

    /* One byte past the last block = data_head position: inside the arena
     * (premise guarantees spare space), outside every live block. */
    uint8_t *past_head = last + AlignUp(1u);
    uint8_t src[1] = { 0 };
    ASSERT_EQ(ds_safe_memory_copy(&buf_, past_head, src, sizeof(src)), ERROR_DS_ALLOCATOR_NOT_FOUND);
}

TEST_F(Safe_Memory_Copy_Tests, Does_Not_Touch_Neighbour_Block)
{
    const size_t len = AlignUp(16u);
    uint8_t *p1 = nullptr;
    uint8_t *p2 = nullptr;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p1), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p2), len), ERROR_DS_OK);
    std::memset(p2, kPrefill, len);

    uint8_t src[AlignUp(16u)];
    std::memset(src, kPattern, sizeof(src));
    ASSERT_EQ(ds_safe_memory_copy(&buf_, p1, src, len), ERROR_DS_OK); /* exact fit */

    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p2[i], kPrefill) << "neighbour byte " << i << " clobbered";
    }
}
