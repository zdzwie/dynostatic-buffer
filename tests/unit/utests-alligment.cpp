/**
 * @file utests-alligment.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for alligment behaviour.
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

/* Fallback if the implementation does not yet expose its alignment. */
#ifndef DS_ALIGNMENT
    #define DS_ALIGNMENT (alignof(max_align_t))
#endif

namespace {
inline bool is_aligned(const void *p, size_t a)
{
    return (reinterpret_cast<uintptr_t>(p) % a) == 0u;
}

inline size_t align_up(size_t v, size_t a)
{
    return (v + (a - 1u)) & ~(a - 1u);
}
} // namespace

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Alignment_Tests, Pointer_Is_Aligned)
   {
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p = NULL;

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);

    // Odd size on purpose: the allocator must still hand back an aligned ptr.
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p), 1), ERROR_DS_OK);
    ASSERT_TRUE(is_aligned(p, DS_ALIGNMENT));

    ds_deinit_allocation(&ds_buffer);
   }
 */

/* TODO: Fix/implement in DS-Buffer and uncomment
   TEST(Alignment_Tests, Consecutive_Stride_Is_Aligned)
   {
    dynostatic_buffer_t ds_buffer = { 0 };
    char *p1 = NULL;
    char *p2 = NULL;
    const size_t len = 10; // not a multiple of DS_ALIGNMENT

    ASSERT_EQ(ds_initialize_allocation(&ds_buffer), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p1), len), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&ds_buffer, reinterpret_cast<void **>(&p2), len), ERROR_DS_OK);

    // Second block starts at the aligned end of the first, not raw len bytes.
    ASSERT_EQ(static_cast<size_t>(p2 - p1), align_up(len, DS_ALIGNMENT));
    ASSERT_TRUE(is_aligned(p2, DS_ALIGNMENT));

    ds_deinit_allocation(&ds_buffer);
   }
 */
