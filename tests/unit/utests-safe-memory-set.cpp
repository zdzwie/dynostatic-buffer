/**
 * @file utests-safe-memory-set.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_safe_memory_set — bounded fill of live blocks.
 * @version 0.1
 * @date 2026-07-12
 */
#include "utests-common.hpp"

using dstest::AlignUp;
using dstest::DsBufferTest;

namespace {
constexpr uint8_t kPrefill = 0xEEu;
constexpr uint8_t kValue = 0xA5u;
} // namespace

class Safe_Memory_Set_Tests : public DsBufferTest {};

/*--------------------- Validation paths ---------------------*/

TEST(Safe_Memory_Set_NoFixture_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t dst_dummy = 0;
    ASSERT_EQ(ds_safe_memory_set(&ds_buffer, &dst_dummy, kValue, 4u), ERROR_DS_NO_INIT);
}

TEST_F(Safe_Memory_Set_Tests, Bad_Input_Params)
{
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), AlignUp(8u)), ERROR_DS_OK);

    ASSERT_EQ(ds_safe_memory_set(NULL, p, kValue, 4u), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_safe_memory_set(&buf_, NULL, kValue, 4u), ERROR_DS_INVALID_ARG);
    ASSERT_EQ(ds_safe_memory_set(&buf_, p, kValue, 0u), ERROR_DS_INVALID_ARG);
}

/*--------------------- Happy paths ---------------------*/

TEST_F(Safe_Memory_Set_Tests, Fills_Block_Start_With_Value)
{
    const size_t len = AlignUp(16u); /* aligned request -> capacity == len */
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    const size_t fill_len = 8u;
    ASSERT_EQ(ds_safe_memory_set(&buf_, p, kValue, fill_len), ERROR_DS_OK);

    for (size_t i = 0; i < fill_len; i++) {
        ASSERT_EQ(p[i], kValue) << "byte " << i;
    }
    for (size_t i = fill_len; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "byte " << i << " beyond the fill clobbered";
    }
}

/* Variant-B discriminator: fill starting at an (unaligned) interior
 * address; both flanks of the filled range must stay intact. */
TEST_F(Safe_Memory_Set_Tests, Fills_Block_Interior)
{
    const size_t len = AlignUp(16u);
    const size_t shift = 3u;
    const size_t fill_len = 5u;
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    ASSERT_EQ(ds_safe_memory_set(&buf_, p + shift, kValue, fill_len), ERROR_DS_OK);

    for (size_t i = 0; i < shift; i++) {
        ASSERT_EQ(p[i], kPrefill) << "prefix byte " << i << " clobbered";
    }
    for (size_t i = shift; i < shift + fill_len; i++) {
        ASSERT_EQ(p[i], kValue) << "filled byte " << i;
    }
    for (size_t i = shift + fill_len; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "suffix byte " << i << " clobbered";
    }
}

/* Value 0x00 must behave identically — guards against any future "zero
 * means something special" shortcut in the ds_memset family. */
TEST_F(Safe_Memory_Set_Tests, Fills_With_Zero_Value)
{
    const size_t len = AlignUp(8u);
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    ASSERT_EQ(ds_safe_memory_set(&buf_, p, 0x00u, len), ERROR_DS_OK);
    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p[i], 0x00u) << "byte " << i;
    }
}

TEST_F(Safe_Memory_Set_Tests, Exact_Fit_To_Block_End)
{
    const size_t len = AlignUp(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);

    /* shift + (len - shift) == capacity: the boundary case that must PASS. */
    ASSERT_EQ(ds_safe_memory_set(&buf_, p + shift, kValue, len - shift), ERROR_DS_OK);
    ASSERT_EQ(p[len - 1u], kValue) << "last byte of the block not written";
}

/*--------------------- Bounds violations ---------------------*/

TEST_F(Safe_Memory_Set_Tests, Rejects_One_Byte_Past_End)
{
    const size_t len = AlignUp(16u);
    const size_t shift = 3u;
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    ASSERT_EQ(ds_safe_memory_set(&buf_, p + shift, kValue, (len - shift) + 1u), ERROR_DS_NO_MEMORY);

    /* Rejection must be side-effect free. */
    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "byte " << i << " modified by a rejected call";
    }
}

/* Overflow guard: shift + cnt wrapping around SIZE_MAX must not slip past
 * the bounds check (naive `shift + cnt > capacity` passes these inputs). */
TEST_F(Safe_Memory_Set_Tests, Rejects_Wrapping_Count)
{
    const size_t len = AlignUp(16u);
    uint8_t *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    std::memset(p, kPrefill, len);

    ASSERT_EQ(ds_safe_memory_set(&buf_, p, kValue, SIZE_MAX), ERROR_DS_NO_MEMORY);
    ASSERT_EQ(ds_safe_memory_set(&buf_, p + 3, kValue, SIZE_MAX - 2u), ERROR_DS_NO_MEMORY);

    for (size_t i = 0; i < len; i++) {
        ASSERT_EQ(p[i], kPrefill) << "byte " << i << " modified by a rejected call";
    }
}

/*--------------------- Lookup outcome spot-checks ---------------------*/
/* The full lookup-branch map for ds_find_allocator_containing lives in
 * utests-safe-memory-copy.cpp; here only the dispatch of THIS public
 * function into the lookup is verified. */

TEST_F(Safe_Memory_Set_Tests, Rejects_Foreign_Pointer)
{
    uint8_t stack_var = 0;
    ASSERT_EQ(ds_safe_memory_set(&buf_, &stack_var, kValue, 1u), ERROR_DS_MEMORY_OUT_OF_DS);
}

TEST_F(Safe_Memory_Set_Tests, Rejects_Pointer_In_Parked_Block)
{
    const size_t len = AlignUp(16u);
    uint8_t *p = NULL;
    uint8_t *guard = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&guard), 1u), ERROR_DS_OK); /* pins the head */

    uint8_t *alias = p;
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK); /* parked DS_FREE */

    ASSERT_EQ(ds_safe_memory_set(&buf_, alias + 2, kValue, 4u), ERROR_DS_ALLOCATOR_NOT_FOUND);
}
