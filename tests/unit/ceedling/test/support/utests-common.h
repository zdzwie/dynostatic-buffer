/**
 * @file utests-common.h
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Shared fixture and helpers for the Ceedling/Unity unit tests.
 *
 * Pure-C counterpart of tests/unit/utests-common.hpp. GoogleTest gives every
 * TEST_F its own fixture object; Unity has a single setUp()/tearDown() pair
 * per test file, so each test file that needs the `DsBufferTest` fixture
 * declares one file-scope `buf_` and forwards setUp()/tearDown() to
 * DsBufferTestSetUp()/DsBufferTestTearDown(). Those two functions must be
 * spelled out in the test file rather than hidden behind a macro: Ceedling's
 * runner generator greps the test source for them and emits empty stubs of
 * its own when it does not find them.
 *
 * @version 1.0
 * @date 2026-09-04
 */
#ifndef UTESTS_COMMON_H
#define UTESTS_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dynostatic-buffer.h"
#include "unity.h"

/**
 * @brief Round @p v up to the next DS_ALIGNMENT multiple.
 *
 * A macro rather than a function so it can size arrays, the way the
 * constexpr dstest::AlignUp() does in the GoogleTest suite.
 */
#define DS_TEST_ALIGN_UP(v) ((((size_t)(v)) + (DS_ALIGNMENT - 1u)) & ~((size_t)(DS_ALIGNMENT - 1u)))

/** Scratch space behind DS_TEST_MSG(); one message is live at a time. */
extern char ds_test_msg_buffer[256];

/**
 * @brief Build a formatted failure message for the *_MESSAGE assertions.
 *
 * Unity takes a plain `const char *`, so it has no equivalent of the
 * `<< "iteration " << i` stream suffixes used throughout the GoogleTest
 * suite; this macro reproduces them.
 */
#define DS_TEST_MSG(...)                                                          \
    ((void)snprintf(ds_test_msg_buffer, sizeof(ds_test_msg_buffer), __VA_ARGS__), \
     (const char *)ds_test_msg_buffer)

/**
 * @brief Zero and initialize @p p_buffer, asserting the init succeeded.
 *
 * @param[out] p_buffer Buffer to bring up.
 */
void DsBufferTestSetUp(dynostatic_buffer_t *p_buffer);

/**
 * @brief Deinitialize @p p_buffer, ignoring the result.
 *
 * Runs even when a test aborted mid-way (Unity always calls tearDown), so
 * deinit is never skipped.
 *
 * @param[in,out] p_buffer Buffer to tear down.
 */
void DsBufferTestTearDown(dynostatic_buffer_t *p_buffer);

/**
 * @brief Check that @p p satisfies DS_ALIGNMENT.
 *
 * @param[in] p Pointer to check.
 *
 * @return true when the pointer is aligned.
 */
bool IsAligned(const void *p);

/**
 * @brief Expected ds_get_memory_usage() result for a set of live allocations.
 *
 * @param[in] total_aligned_bytes Total count of aligned bytes allocated in the buffer.
 *
 * @return Expected memory usage in %.
 */
uint8_t ExpectedUsage(size_t total_aligned_bytes);

/**
 * @brief Allocation helper for tests where malloc itself is not under test.
 *
 * @param[in,out] p_buffer Initialized buffer.
 * @param[in]     size     Requested size in bytes.
 *
 * @return Pointer to the fresh block (NULL on failure, which also fails the test).
 */
void *DsTestMalloc(dynostatic_buffer_t *p_buffer, size_t size);

#endif /* UTESTS_COMMON_H */
