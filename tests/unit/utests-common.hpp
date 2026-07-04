/**
 * @file utests-common.hpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Shared fixture and helpers for dynostatic-buffer unit tests.
 * @version 0.1
 * @date 2026-07-03
 */
#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <cstddef>
#include <cstring>

extern "C" {
#include "dynostatic-buffer.h"
#include "error.h"
}

/*
 * Alignment contract of this test suite:
 *  - if the implementation defines DS_ALIGNMENT, all size arithmetic in the
 *    tests is rounded up to it (AlignUp);
 *  - fallback of 1 models the pre-alignment implementation: AlignUp becomes
 *    the identity, so the very same suite passes both BEFORE and AFTER the
 *    alignment fix lands. Alignment-specific tests detect the fallback and
 *    GTEST_SKIP themselves (see utests-alignment.cpp).
 */
#ifdef DS_ALIGNMENT
    #define DS_TEST_HAS_ALIGNMENT 1
#else
    #define DS_TEST_HAS_ALIGNMENT 0
#endif

namespace dstest {

#if DS_TEST_HAS_ALIGNMENT

inline bool IsAligned(const void *p)
{
    return (reinterpret_cast<uintptr_t>(p) % DS_ALIGNMENT) == 0u;
}

inline size_t AlignUp(size_t v)
{
    return (v + (DS_ALIGNMENT - 1u)) & ~static_cast<size_t>(DS_ALIGNMENT - 1u);
}

#else /* pre-alignment implementation */

inline bool IsAligned(const void *)
{
    return true;
}

inline size_t AlignUp(size_t v)
{
    return v;
}

#endif /* DS_TEST_HAS_ALIGNMENT */

/**
     * @brief Expected ds_get_memory_usage() result for a set of live allocations.
     *
     * @param[in] total_aligned_bytes Total count of aligned bytes allocated in the buffer.
     *
    * @return Expected memory usage in %.
     */
inline uint8_t ExpectedUsage(size_t total_aligned_bytes)
{
    return static_cast<uint8_t>((100u * total_aligned_bytes) / DS_BUFFER_MEMORY_SIZE);
}

/**
 * @class DsBufferTest
 * @brief Fixture with an initialized buffer.
 *
 * TearDown runs even when an ASSERT_* aborts the test body, so deinit is
 * never skipped and no test leaks state into the next one.
 */
class DsBufferTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        /* Explicit zeroing: the 'initialized' flag of an automatic-storage
         * struct is indeterminate otherwise. */
        std::memset(&buf_, 0, sizeof(buf_));
        ASSERT_EQ(ds_initialize_allocation(&buf_), ERROR_DS_OK);
    }

    void TearDown() override
    {
        (void)ds_deinit_allocation(&buf_);
    }

    /** Allocation helper for tests where malloc itself is not under test. */
    void *Malloc(size_t size)
    {
        void *p = nullptr;
        EXPECT_EQ(ds_malloc(&buf_, &p, size), ERROR_DS_OK);
        return p;
    }

    dynostatic_buffer_t buf_{};
};

} // namespace dstest
