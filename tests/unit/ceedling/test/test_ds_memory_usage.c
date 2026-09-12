/**
 * @file test_ds_memory_usage.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_get_memory_usage function behaviour.
 *
 * Ceedling/Unity port of tests/unit/utests-memory-usage.cpp
 * (MemoryUsage_Tests and MemoryUsage_NoFixture_Tests).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"

static dynostatic_buffer_t buf_;

void setUp(void)
{
    DsBufferTestSetUp(&buf_);
}

void tearDown(void)
{
    DsBufferTestTearDown(&buf_);
}

/*---------------------------- Local helpers ----------------------------*/

/** Round @p v DOWN to the previous DS_ALIGNMENT multiple. */
#define DS_TEST_ALIGN_DOWN(v) (((size_t)(v)) & ~((size_t)(DS_ALIGNMENT - 1u)))

/**
 * @brief The percentage expression as it stood BEFORE the overflow fix,
 *        evaluated with the integer widths of a 16-bit target.
 *
 * The original line was
 *
 *     *p_memory_usage = (uint8_t)((100u * usage) / DS_BUFFER_MEMORY_SIZE);
 *
 * where `usage` is size_t. On a target with a 16-bit size_t (MSP430, PIC24,
 * and most small-model embedded compilers) `100u` is also 16 bits, so the
 * usual arithmetic conversions leave the product in a 16-bit type and it
 * wraps for any live capacity above 655 bytes. The explicit uint16_t cast
 * below reproduces that truncation on a host where it would not otherwise
 * happen.
 *
 * @param[in] live_bytes Total live capacity in bytes.
 * @param[in] arena      Arena size in bytes.
 *
 * @return The percentage the pre-fix code would have produced.
 */
static uint8_t UsagePercentPreFix16Bit(uint16_t live_bytes, uint16_t arena)
{
    return (uint8_t)((uint16_t)(100u * live_bytes) / arena);
}

/**
 * @brief The current expression, evaluated with the same 16-bit inputs.
 *
 * @param[in] live_bytes Total live capacity in bytes.
 * @param[in] arena      Arena size in bytes.
 *
 * @return The percentage the current code produces.
 */
static uint8_t UsagePercentPostFix16Bit(uint16_t live_bytes, uint16_t arena)
{
    return (uint8_t)(((uint32_t)live_bytes * 100u) / (uint32_t)arena);
}

/**
 * @brief Read the usage percentage, asserting the call itself succeeded.
 *
 * @return Reported occupancy in percent.
 */
static uint8_t Usage(void)
{
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    return usage;
}

/**
 * @brief Allocate until the bump space is exhausted, in the largest chunks
 *        the configuration permits.
 */
static void FillArenaCompletely(void)
{
    size_t remaining = DS_BUFFER_MEMORY_SIZE;
    size_t chunks = 0u;

    while (remaining > 0u) {
        void *p = NULL;
        const size_t chunk = (remaining > DS_MAX_ALLOCATION_SIZE)
                               ? (size_t)DS_MAX_ALLOCATION_SIZE
                               : remaining;

        TEST_ASSERT_EQUAL_size_t_MESSAGE(chunk, DS_TEST_ALIGN_UP(chunk),
                                         "premise: the arena must decompose into aligned chunks");
        TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ds_malloc(&buf_, &p, chunk),
                                       DS_TEST_MSG("filling the arena, remaining %zu", remaining));

        remaining -= chunk;
        chunks++;
        TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE((size_t)DS_MAX_ALLOCATION_COUNT, chunks,
                                                 "premise: enough allocator slots to fill the bump space");
    }
}

/*--------------- Fixture-free tests (uninitialized buffer) ---------------*/

void test_MemoryUsage_No_Init(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_NO_INIT, ds_get_memory_usage(&ds_buffer, &usage));
}

void test_MemoryUsage_Null_Buffer(void)
{
    uint8_t usage = 0xFFu;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_memory_usage(NULL, &usage));
}

/*--------------------------- Fixture tests ---------------------------*/

void test_MemoryUsage_Null_Output(void)
{
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_INVALID_ARG, ds_get_memory_usage(&buf_, NULL));
}

/*----------------- Percentage value contract (0..100) ------------------*/

void test_MemoryUsage_Empty_Buffer_Is_Zero_Percent(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, Usage());
}

/**
 * Regression test for the percentage overflow.
 *
 * This is the value the pre-fix expression got wrong: with the shipped
 * 1024-byte arena a 16-bit target computed 100 * 1024 = 102400, wrapped to
 * 36864, and reported 36 % for a completely full buffer while still
 * returning ERROR_DS_OK. Exact equality, not a tolerance: a full arena is
 * exactly 100 % by definition and there is nothing to round.
 *
 * @note A host with a 32- or 64-bit size_t cannot reproduce the wrap, so on
 *       the CI runners this asserts the contract rather than catching the
 *       defect. It is the test that fails on an affected target, which is
 *       why it is written against the library rather than the arithmetic;
 *       test_MemoryUsage_Percentage_Arithmetic_Is_Width_Independent below
 *       pins the formula shape on every host.
 */
void test_MemoryUsage_Full_Buffer_Is_100_Percent(void)
{
    FillArenaCompletely();

    TEST_ASSERT_EQUAL_UINT8(100u, Usage());
}

/**
 * Walk the whole 0..100 range in equal steps, checking exactness at each
 * one. Any future rewrite of the percentage line that is off by a rounding
 * mode, a truncation, or an integer width fails here for some fill level
 * even if the endpoints happen to survive.
 */
void test_MemoryUsage_Is_Exact_And_Monotonic_Across_Fill_Levels(void)
{
    const size_t c_chunk = DS_TEST_ALIGN_DOWN(DS_BUFFER_MEMORY_SIZE / DS_MAX_ALLOCATION_COUNT);
    size_t live_capacity = 0u;
    uint8_t previous = 0u;
    size_t step;

    TEST_ASSERT_GREATER_OR_EQUAL_size_t_MESSAGE(
        (size_t)DS_ALIGNMENT, c_chunk,
        "premise: the arena splits into DS_MAX_ALLOCATION_COUNT aligned chunks");
    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE(
        (size_t)DS_MAX_ALLOCATION_SIZE, c_chunk,
        "premise: one chunk is a legal allocation size");

    for (step = 0u; step < DS_MAX_ALLOCATION_COUNT; step++) {
        void *p = NULL;
        uint8_t usage;

        TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ds_malloc(&buf_, &p, c_chunk),
                                       DS_TEST_MSG("step %zu", step));

        live_capacity += DS_TEST_ALIGN_UP(c_chunk);

        usage = Usage();
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ExpectedUsage(live_capacity), usage,
                                        DS_TEST_MSG("step %zu, live capacity %zu", step, live_capacity));
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(previous, usage,
                                                   DS_TEST_MSG("usage must not fall while allocating, step %zu", step));
        TEST_ASSERT_LESS_OR_EQUAL_UINT8_MESSAGE(100u, usage, DS_TEST_MSG("step %zu", step));

        previous = usage;
    }
}

/**
 * The documented contract is "rounded down", and it is the rounding the
 * percentage line performs. Pin it, so a well-meaning change to that line
 * cannot silently switch to rounding up or to nearest.
 */
void test_MemoryUsage_Rounds_Down(void)
{
    const size_t c_capacity = DS_TEST_ALIGN_UP(1u);
    void *p = NULL;

    TEST_ASSERT_LESS_THAN_size_t_MESSAGE((size_t)DS_BUFFER_MEMORY_SIZE, c_capacity * 100u,
                                         "premise: one minimal block is well under 1 % of the arena");

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, &p, 1u));

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, Usage(),
                                    "a live sub-1 % block must round down to 0, not up to 1");
}

/**
 * Usage sums PHYSICAL CAPACITIES, not requested sizes. A request that is not
 * a multiple of DS_ALIGNMENT must therefore report the rounded-up figure.
 */
void test_MemoryUsage_Counts_Capacity_Not_Requested_Size(void)
{
    const size_t c_request = (size_t)DS_MAX_ALLOCATION_SIZE - 1u;
    void *p = NULL;

    TEST_ASSERT_NOT_EQUAL_size_t_MESSAGE(c_request, DS_TEST_ALIGN_UP(c_request),
                                         "premise: the request must be unaligned for this test to mean anything");

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, &p, c_request));

    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(DS_TEST_ALIGN_UP(c_request)), Usage());
}

/**
 * A parked DS_FREE block counts as free space, per the documented contract.
 */
void test_MemoryUsage_Parked_Free_Block_Counts_As_Free(void)
{
    const size_t c_chunk = (size_t)DS_MAX_ALLOCATION_SIZE;
    void *first = NULL;
    void *second = NULL;
    uint8_t both_live;

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, &first, c_chunk));
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_malloc(&buf_, &second, c_chunk));

    both_live = Usage();
    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(2u * DS_TEST_ALIGN_UP(c_chunk)), both_live);

    /* Freeing the FIRST block parks it (it is not the trailing block), so the
     * bump head does not move but the capacity must stop being counted. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, &first));

    TEST_ASSERT_EQUAL_UINT8(ExpectedUsage(DS_TEST_ALIGN_UP(c_chunk)), Usage());
    TEST_ASSERT_LESS_THAN_UINT8(both_live, Usage());
}

/*------------------- Overflow guard on the arithmetic -------------------*/

/*
 * Mirrors the DS_STATIC_ASSERT added to the public header alongside the fix.
 * Restating it here means a configuration change that outgrows the 32-bit
 * product is caught by the test suite too, not only by whoever happens to
 * recompile the library.
 */
DS_STATIC_ASSERT(DS_BUFFER_MEMORY_SIZE <= (UINT32_MAX / 100u),
                 "DS_BUFFER_MEMORY_SIZE too large for the usage percentage arithmetic");

/**
 * Guards the SHAPE of the percentage expression against a revert.
 *
 * ds_get_memory_usage() cannot be made to overflow on a host where size_t is
 * 32 or 64 bits, so no call into the library can distinguish the pre- and
 * post-fix code here — the two forms agree for every input the host can
 * represent. What this test does instead is evaluate both forms at the
 * integer width of the targets that are affected, documenting the defect and
 * failing if the widening cast is ever dropped from either the library or
 * ExpectedUsage().
 */
void test_MemoryUsage_Percentage_Arithmetic_Is_Width_Independent(void)
{
    uint32_t live;

    /* The historical failure, in the shipped 1024-byte configuration:
     * 100 * 1024 = 102400, which wraps to 36864 in 16 bits, so a completely
     * full arena reported 36 % instead of 100 %. Fixed literals rather than
     * DS_BUFFER_MEMORY_SIZE: this documents one concrete past defect and must
     * not drift with the configuration. */
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(36u, UsagePercentPreFix16Bit(1024u, 1024u),
                                    "the defect this test guards against");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(100u, UsagePercentPostFix16Bit(1024u, 1024u),
                                    "a full arena is 100 %, at any integer width");

    /* The wrap begins as soon as the product leaves 16 bits, at 656 bytes
     * (100 * 656 = 65600 > UINT16_MAX). Below that the two forms agree, which
     * is precisely why the bug survived the existing tests for so long. */
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UsagePercentPostFix16Bit(655u, 1024u),
                                    UsagePercentPreFix16Bit(655u, 1024u),
                                    "premise: the forms are indistinguishable below the wrap point");
    TEST_ASSERT_NOT_EQUAL_UINT8_MESSAGE(UsagePercentPostFix16Bit(656u, 1024u),
                                        UsagePercentPreFix16Bit(656u, 1024u),
                                        "premise: 656 bytes is the first live capacity that overflows 16 bits");

    /* The current form stays exact across the entire domain, at 16-bit width. */
    for (live = 0u; live <= 1024u; live += DS_ALIGNMENT) {
        const uint8_t expected = (uint8_t)((live * 100u) / 1024u);

        TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected, UsagePercentPostFix16Bit((uint16_t)live, 1024u),
                                        DS_TEST_MSG("live capacity %u", (unsigned)live));
    }
}
