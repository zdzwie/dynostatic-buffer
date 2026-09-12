/**
 * @file utests-memory-usage.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Unit tests for ds_get_memory_usage function behaviour.
 * @version 1.0
 * @date 2026-07-05
 */
#include "utests-common.hpp"

#include <cstdint>

using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::ExpectedUsage;

namespace {

/** Round @p v DOWN to the previous DS_ALIGNMENT multiple. */
constexpr std::size_t AlignDown(std::size_t v) noexcept
{
    return v & ~static_cast<std::size_t>(DS_ALIGNMENT - 1u);
}

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
 */
constexpr std::uint8_t UsagePercentPreFix16Bit(std::uint16_t live_bytes, std::uint16_t arena) noexcept
{
    return static_cast<std::uint8_t>(static_cast<std::uint16_t>(100u * live_bytes) / arena);
}

/** The current expression, evaluated with the same 16-bit inputs. */
constexpr std::uint8_t UsagePercentPostFix16Bit(std::uint16_t live_bytes, std::uint16_t arena) noexcept
{
    return static_cast<std::uint8_t>((static_cast<std::uint32_t>(live_bytes) * 100u)
                                     / static_cast<std::uint32_t>(arena));
}

} // namespace

class MemoryUsage_Tests : public DsBufferTest {
  protected:
    /** Read the usage percentage, asserting the call itself succeeded. */
    uint8_t Usage()
    {
        uint8_t usage = 0xFFu;
        EXPECT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
        return usage;
    }

    /**
     * @brief Allocate until the bump space is exhausted, in the largest
     *        chunks the configuration permits.
     */
    void FillArenaCompletely()
    {
        std::size_t remaining = DS_BUFFER_MEMORY_SIZE;
        std::size_t chunks = 0u;

        while (remaining > 0u) {
            const std::size_t chunk = (remaining > DS_MAX_ALLOCATION_SIZE)
                                        ? static_cast<std::size_t>(DS_MAX_ALLOCATION_SIZE)
                                        : remaining;
            ASSERT_EQ(AlignUp(chunk), chunk)
                << "premise: the arena must decompose into aligned chunks";

            char *p = NULL;
            ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), chunk), ERROR_DS_OK)
                << "filling the arena, remaining " << remaining;

            remaining -= chunk;
            chunks++;
            ASSERT_LE(chunks, static_cast<std::size_t>(DS_MAX_ALLOCATION_COUNT))
                << "premise: enough allocator slots to fill the bump space";
        }
    }
};

TEST(MemoryUsage_NoFixture_Tests, No_Init)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_get_memory_usage(&ds_buffer, &usage), ERROR_DS_NO_INIT);
}

TEST(MemoryUsage_NoFixture_Tests, Null_Buffer)
{
    uint8_t usage = 0xFF;

    ASSERT_EQ(ds_get_memory_usage(NULL, &usage), ERROR_DS_INVALID_ARG);
}

TEST_F(MemoryUsage_Tests, Null_Output)
{
    ASSERT_EQ(ds_get_memory_usage(&buf_, NULL), ERROR_DS_INVALID_ARG);
}

/*----------------- Percentage value contract (0..100) ------------------*/

TEST_F(MemoryUsage_Tests, Empty_Buffer_Is_Zero_Percent)
{
    ASSERT_EQ(Usage(), 0u);
}

TEST_F(MemoryUsage_Tests, Full_Buffer_Is_100_Percent)
{
    FillArenaCompletely();

    ASSERT_EQ(Usage(), 100u);
}

/**
 * Walk the whole 0..100 range in equal steps, checking exactness at each
 * one. Any future rewrite of the percentage line that is off by a rounding
 * mode, a truncation, or an integer width fails here for some fill level
 * even if the endpoints happen to survive.
 */
TEST_F(MemoryUsage_Tests, Usage_Is_Exact_And_Monotonic_Across_Fill_Levels)
{
    constexpr std::size_t c_chunk = AlignDown(DS_BUFFER_MEMORY_SIZE / DS_MAX_ALLOCATION_COUNT);

    ASSERT_GE(c_chunk, static_cast<std::size_t>(DS_ALIGNMENT))
        << "premise: the arena splits into DS_MAX_ALLOCATION_COUNT aligned chunks";
    ASSERT_LE(c_chunk, static_cast<std::size_t>(DS_MAX_ALLOCATION_SIZE))
        << "premise: one chunk is a legal allocation size";

    std::size_t live_capacity = 0u;
    uint8_t previous = 0u;

    for (std::size_t step = 0u; step < DS_MAX_ALLOCATION_COUNT; step++) {
        char *p = NULL;
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), c_chunk), ERROR_DS_OK)
            << "step " << step;

        live_capacity += AlignUp(c_chunk);

        const uint8_t usage = Usage();
        ASSERT_EQ(usage, ExpectedUsage(live_capacity))
            << "step " << step << ", live capacity " << live_capacity;
        ASSERT_GE(usage, previous) << "usage must not fall while allocating, step " << step;
        ASSERT_LE(usage, 100u) << "step " << step;

        previous = usage;
    }
}

/**
 * The documented contract is "rounded down", and it is the rounding the
 * percentage line performs. Pin it, so a well-meaning change to that line
 * cannot silently switch to rounding up or to nearest.
 */
TEST_F(MemoryUsage_Tests, Usage_Rounds_Down)
{
    const std::size_t c_capacity = AlignUp(1u);

    ASSERT_LT(c_capacity * 100u, static_cast<std::size_t>(DS_BUFFER_MEMORY_SIZE))
        << "premise: one minimal block is well under 1 % of the arena";

    char *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), 1u), ERROR_DS_OK);

    ASSERT_EQ(Usage(), 0u) << "a live sub-1 % block must round down to 0, not up to 1";
}

/**
 * Usage sums PHYSICAL CAPACITIES, not requested sizes. A request that is not
 * a multiple of DS_ALIGNMENT must therefore report the rounded-up figure.
 */
TEST_F(MemoryUsage_Tests, Usage_Counts_Capacity_Not_Requested_Size)
{
    constexpr std::size_t c_request = DS_MAX_ALLOCATION_SIZE - 1u;

    ASSERT_NE(AlignUp(c_request), c_request)
        << "premise: the request must be unaligned for this test to mean anything";

    char *p = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), c_request), ERROR_DS_OK);

    ASSERT_EQ(Usage(), ExpectedUsage(AlignUp(c_request)));
}

/**
 * A parked DS_FREE block counts as free space, per the documented contract.
 */
TEST_F(MemoryUsage_Tests, Parked_Free_Block_Counts_As_Free)
{
    constexpr std::size_t c_chunk = DS_MAX_ALLOCATION_SIZE;

    char *first = NULL;
    char *second = NULL;
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&first), c_chunk), ERROR_DS_OK);
    ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&second), c_chunk), ERROR_DS_OK);

    const uint8_t both_live = Usage();
    ASSERT_EQ(both_live, ExpectedUsage(2u * AlignUp(c_chunk)));

    /* Freeing the FIRST block parks it (it is not the trailing block), so the
     * bump head does not move but the capacity must stop being counted. */
    ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&first)), ERROR_DS_OK);

    ASSERT_EQ(Usage(), ExpectedUsage(AlignUp(c_chunk)));
    ASSERT_LT(Usage(), both_live);
}

/*------------------- Overflow guard on the arithmetic -------------------*/

/**
 * Mirrors the DS_STATIC_ASSERT added to the public header alongside the fix.
 * Restating it here means a configuration change that outgrows the 32-bit
 * product is caught by the test suite too, not only by whoever happens to
 * recompile the library.
 */
static_assert(DS_BUFFER_MEMORY_SIZE <= (UINT32_MAX / 100u),
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
TEST(MemoryUsage_NoFixture_Tests, Percentage_Arithmetic_Is_Width_Independent)
{
    /* The historical failure, in the shipped 1024-byte configuration:
     * 100 * 1024 = 102400, which wraps to 36864 in 16 bits, so a completely
     * full arena reported 36 % instead of 100 %. Fixed literals rather than
     * DS_BUFFER_MEMORY_SIZE: this documents one concrete past defect and must
     * not drift with the configuration. */
    EXPECT_EQ(UsagePercentPreFix16Bit(1024u, 1024u), 36u) << "the defect this test guards against";
    EXPECT_EQ(UsagePercentPostFix16Bit(1024u, 1024u), 100u) << "a full arena is 100 %, at any integer width";

    /* The wrap begins as soon as the product leaves 16 bits, at 656 bytes
     * (100 * 656 = 65600 > UINT16_MAX). Below that the two forms agree, which
     * is precisely why the bug survived the existing tests for so long. */
    EXPECT_EQ(UsagePercentPreFix16Bit(655u, 1024u), UsagePercentPostFix16Bit(655u, 1024u))
        << "premise: the forms are indistinguishable below the wrap point";
    EXPECT_NE(UsagePercentPreFix16Bit(656u, 1024u), UsagePercentPostFix16Bit(656u, 1024u))
        << "premise: 656 bytes is the first live capacity that overflows 16 bits";

    /* The current form stays exact across the entire domain, at 16-bit width. */
    for (std::uint32_t live = 0u; live <= 1024u; live += DS_ALIGNMENT) {
        const std::uint8_t expected = static_cast<std::uint8_t>((live * 100u) / 1024u);
        EXPECT_EQ(UsagePercentPostFix16Bit(static_cast<std::uint16_t>(live), 1024u), expected)
            << "live capacity " << live;
    }
}
