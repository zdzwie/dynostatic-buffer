/**
 * @file test_ds_alignment.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for alignment behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-alligment.cpp (Alignment_Tests and
 * the Various_Sizes instantiation of Alignment_Param_Tests). Unity has no
 * value-parameterized tests, so the parameter list becomes an array and each
 * iteration re-runs the fixture to get the pristine buffer GoogleTest would
 * have handed to a fresh TEST_P instance.
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

/** Sizes from INSTANTIATE_TEST_SUITE_P(Various_Sizes, ...). */
static const size_t various_sizes[] = { 1u,
                                        2u,
                                        3u,
                                        (size_t)DS_ALIGNMENT,
                                        (size_t)DS_ALIGNMENT + 1u,
                                        (2u * DS_ALIGNMENT) - 1u,
                                        2u * DS_ALIGNMENT };

static dynostatic_buffer_t buf_;

void setUp(void)
{
    DsBufferTestSetUp(&buf_);
}

void tearDown(void)
{
    DsBufferTestTearDown(&buf_);
}

/*--------------------- Parameterized alignment checks ---------------------*/

void test_Alignment_Malloc_Returns_Aligned_Pointer(void)
{
    size_t idx;

    for (idx = 0u; idx < (sizeof(various_sizes) / sizeof(various_sizes[0])); idx++) {
        void *p = NULL;

        /* Fresh buffer per parameter, as TEST_P would give. */
        DsBufferTestTearDown(&buf_);
        DsBufferTestSetUp(&buf_);

        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, &p, various_sizes[idx]));
        TEST_ASSERT_TRUE_MESSAGE(IsAligned(p), DS_TEST_MSG("size=%zu", various_sizes[idx]));
    }
}

void test_Alignment_Calloc_Returns_Aligned_Pointer(void)
{
    size_t idx;

    for (idx = 0u; idx < (sizeof(various_sizes) / sizeof(various_sizes[0])); idx++) {
        void *p = NULL;

        DsBufferTestTearDown(&buf_);
        DsBufferTestSetUp(&buf_);

        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_calloc(&buf_, &p, various_sizes[idx], 1u));
        TEST_ASSERT_TRUE_MESSAGE(IsAligned(p), DS_TEST_MSG("len=%zu", various_sizes[idx]));
    }
}

/*--------------------------- Fixture tests ---------------------------*/

void test_Alignment_Consecutive_Stride_Is_Aligned(void)
{
    char *p1 = NULL;
    char *p2 = NULL;
    const size_t len = 10u; /* not a multiple of DS_ALIGNMENT (for align >= 4) */

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, len));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, len));

    /* Second block starts at the aligned end of the first, not raw len bytes. */
    TEST_ASSERT_EQUAL_size_t(DS_TEST_ALIGN_UP(len), (size_t)(p2 - p1));
    TEST_ASSERT_TRUE(IsAligned(p2));
}

void test_Alignment_Minimal_Requests_Consume_Full_Alignment(void)
{
    char *p1 = NULL;
    char *p2 = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, 1));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, 1));

    TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)DS_ALIGNMENT, (size_t)(p2 - p1),
                                     "malloc(1) must physically consume DS_ALIGNMENT bytes");
}

/* Boundary: aligned-size accounting at the end of the buffer. A raw request
 * would fit in the remaining space, but its aligned size does not. */
void test_Alignment_Aligned_Size_Exceeds_Remaining_Space(void)
{
    /* Consume the buffer down to exactly one aligned slot. */
    size_t remaining = DS_BUFFER_MEMORY_SIZE - (DS_BUFFER_MEMORY_SIZE % DS_ALIGNMENT);
    char *p = NULL;

    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        (size_t)DS_MAX_ALLOCATION_SIZE, DS_TEST_ALIGN_UP(DS_MAX_ALLOCATION_SIZE),
        "test premise: DS_MAX_ALLOCATION_SIZE must be alignment-multiple");

    while (remaining > DS_ALIGNMENT) {
        size_t chunk = remaining - DS_ALIGNMENT;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, chunk));
        p = NULL;
        remaining -= chunk;
    }

    /* Exactly DS_ALIGNMENT bytes of aligned space remain: a request of
     * DS_ALIGNMENT + 1 rounds to 2*DS_ALIGNMENT and must be rejected... */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY, ds_malloc(&buf_, (void **)&p, DS_ALIGNMENT + 1));

    /* ...while an unaligned request of 1 byte still fits the last slot. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, 1));
    TEST_ASSERT_TRUE(IsAligned(p));

    /* Buffer is now physically full despite the last request being 1 byte. */
    p = NULL;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_MEMORY, ds_malloc(&buf_, (void **)&p, 1));
}

void test_Alignment_Reused_Block_Is_Aligned(void)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char *guard = NULL;
    char *original = NULL;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p1, 3));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&guard, 1));

    original = p1;
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p1));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, (void **)&p2, 1));
    TEST_ASSERT_TRUE(IsAligned(p2));
    TEST_ASSERT_EQUAL_PTR(original, p2);
}
