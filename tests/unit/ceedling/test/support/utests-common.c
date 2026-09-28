/**
 * @file utests-common.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Functions used by all Ceedling unit test files.
 * @version 1.0
 * @date 2026-09-04
 */

#include "utests-common.h"

char ds_test_msg_buffer[256];

void DsBufferTestSetUp(dynostatic_buffer_t *p_buffer)
{
    /* Explicit zeroing: the init_magic marker of an automatic- or
     * static-storage struct reused across tests is otherwise stale. */
    (void)memset(p_buffer, 0, sizeof(*p_buffer));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_initialize_allocation(p_buffer));
}

void DsBufferTestTearDown(dynostatic_buffer_t *p_buffer)
{
    (void)ds_deinit_allocation(p_buffer);
}

bool IsAligned(const void *p)
{
    return (((uintptr_t)p) % DS_ALIGNMENT) == 0u;
}

uint8_t ExpectedUsage(size_t total_aligned_bytes)
{
    /* Computed deliberately wider than ds_get_memory_usage() does, so this
     * stays an INDEPENDENT oracle. The previous form mirrored the
     * implementation expression character for character, including the
     * 16-bit-size_t overflow it used to contain — an oracle that reproduces
     * the defect can never detect it. See test_ds_memory_usage.c. */
    return (uint8_t)(((unsigned long long)total_aligned_bytes * 100ULL)
                     / (unsigned long long)DS_BUFFER_MEMORY_SIZE);
}

void *DsTestMalloc(dynostatic_buffer_t *p_buffer, size_t size)
{
    void *p = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(p_buffer, &p, size));

    return p;
}
