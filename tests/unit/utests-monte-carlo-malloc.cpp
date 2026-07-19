/**
 * @file utests-monte-carlo-malloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Model-based randomized stress test: a deterministic storm of
 *        malloc/free operations checked against a shadow model, with
 *        ds_get_max_new_allocation_size() as the success oracle and a
 *        no-permanent-fragmentation end-state assertion.
 * @version 0.1
 * @date 2026-07-13
 */
#include "utests-common.hpp"

#include <algorithm>
#include <random>
#include <vector>
#include <numeric>

using dstest::AlignUp;
using dstest::DsBufferTest;
using dstest::ExpectedUsage;
using dstest::IsAligned;

namespace {
constexpr size_t kOperations = 1000u;
constexpr unsigned kMallocBiasPercent = 55u; /* malloc vs free mix while blocks are live */
} // namespace

class Monte_Carlo_Tests : public DsBufferTest,
                          public ::testing::WithParamInterface<uint32_t> {
  protected:
    struct LiveBlock {
        uint8_t *ptr;
        size_t requested;
        uint8_t pattern;
    };

    /* Shadow model: what the test believes is currently allocated. */
    std::vector<LiveBlock> live_;
    uint8_t next_pattern_ = 1u; /* never 0: distinguishes user data from zeroed memory */

    uint8_t NextPattern()
    {
        const uint8_t p = next_pattern_;
        next_pattern_ = static_cast<uint8_t>((next_pattern_ % 250u) + 1u);
        return p;
    }

    void AssertGlobalInvariants(size_t op, uint32_t seed)
    {
        const size_t total_aligned = std::accumulate(
            live_.cbegin(), live_.cend(), size_t{ 0 },
            [](size_t acc, const LiveBlock &b) { return acc + AlignUp(b.requested); });

        uint8_t usage = 0xFFu;
        ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
        /* Reused blocks RETAIN their original capacity (see ds_allocator_t), so
         * real usage may exceed the sum of aligned requests — the model can
         * assert a LOWER BOUND per-op; exact equality holds only at drain. */
        ASSERT_GE(static_cast<unsigned>(usage),
                  static_cast<unsigned>(ExpectedUsage(total_aligned)))
            << "usage below the aligned-requests floor at op " << op << " (seed " << seed << ")";

        size_t cnt = 0xFFu;
        ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
        ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) - live_.size())
            << "slot count drifted from the model at op " << op << " (seed " << seed << ")";
    }

    void AssertNoOverlapWithLive(const uint8_t *p, size_t requested, size_t op, uint32_t seed)
    {
        const uintptr_t a1 = reinterpret_cast<uintptr_t>(p);
        const uintptr_t a2 = a1 + AlignUp(requested);
        for (const LiveBlock &b : live_) {
            const uintptr_t b1 = reinterpret_cast<uintptr_t>(b.ptr);
            const uintptr_t b2 = b1 + AlignUp(b.requested);
            ASSERT_TRUE((a2 <= b1) || (b2 <= a1))
                << "new block overlaps a live block at op " << op << " (seed " << seed << ")";
        }
    }

    void VerifyAndFree(size_t victim, size_t op, uint32_t seed)
    {
        LiveBlock &b = live_[victim];

        /* Cross-corruption detector: the pattern written at allocation time
         * must survive every operation performed since. */
        for (size_t i = 0; i < b.requested; i++) {
            ASSERT_EQ(b.ptr[i], b.pattern)
                << "block corrupted at byte " << i << ", op " << op << " (seed " << seed << ")";
        }

        uint8_t *p = b.ptr;
        ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK)
            << "free of a model-live block failed at op " << op << " (seed " << seed << ")";
        ASSERT_EQ(p, nullptr);

        live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(victim));
    }
};

TEST_P(Monte_Carlo_Tests, Random_Alloc_Free_Storm)
{
    const uint32_t seed = GetParam();
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> size_dist(1u, DS_MAX_ALLOCATION_SIZE);

    for (size_t op = 0u; op < kOperations; op++) {
        const bool do_malloc = live_.empty() || ((rng() % 100u) < kMallocBiasPercent);

        if (do_malloc) {
            const size_t req = size_dist(rng);

            /* Oracle: the getter's tight contract decides the expected
             * outcome BEFORE the call — transient fragmentation is allowed
             * exactly when the oracle says so, and never otherwise. */
            size_t oracle = 0u;
            ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &oracle), ERROR_DS_OK);

            uint8_t *p = NULL;
            const ds_err_code_t ret = ds_malloc(&buf_, reinterpret_cast<void **>(&p), req);

            if (req <= oracle) {
                ASSERT_EQ(ret, ERROR_DS_OK)
                    << "oracle promised " << oracle << " but malloc(" << req
                    << ") failed at op " << op << " (seed " << seed << ")";
                ASSERT_TRUE(IsAligned(p));
                AssertNoOverlapWithLive(p, req, op, seed);

#if DS_ZERO_ON_FREE
                /* Under the zero-on-free policy every delivered block —
                 * fresh or reused — must arrive clean. */
                for (size_t i = 0; i < req; i++) {
                    ASSERT_EQ(p[i], 0u)
                        << "dirty byte " << i << " in a delivered block, op " << op
                        << " (seed " << seed << ")";
                }
#endif

                const uint8_t pattern = NextPattern();
                std::memset(p, pattern, req);
                live_.push_back(LiveBlock{ p, req, pattern });
            } else {
                ASSERT_NE(ret, ERROR_DS_OK)
                    << "malloc(" << req << ") succeeded past the oracle bound " << oracle
                    << " at op " << op << " (seed " << seed << ")";
                ASSERT_EQ(p, nullptr) << "failed malloc modified the out-pointer";
            }
        } else {
            std::uniform_int_distribution<size_t> victim_dist(0u, live_.size() - 1u);
            VerifyAndFree(victim_dist(rng), op, seed);
        }

        AssertGlobalInvariants(op, seed);
    }

    /* Drain in random order: park/cascade interleavings at their messiest. */
    std::shuffle(live_.begin(), live_.end(), rng);
    while (!live_.empty()) {
        VerifyAndFree(live_.size() - 1u, kOperations, seed);
    }

    /* NO PERMANENT FRAGMENTATION: after a full drain the instance must be
     * indistinguishable from a freshly initialized one. */
    uint8_t usage = 0xFFu;
    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(usage, 0u) << "usage did not return to zero (seed " << seed << ")";

    size_t cnt = 0u;
    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT))
        << "allocator slots leaked (seed " << seed << ")";

    size_t max_alloc = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &max_alloc), ERROR_DS_OK);
    ASSERT_EQ(max_alloc, static_cast<size_t>(DS_MAX_ALLOCATION_SIZE))
        << "post-drain capacity below pristine (seed " << seed << ")";

    /* Behavioural proof: the WHOLE arena is allocatable again. */
    size_t refilled = 0u;
    while (refilled < DS_BUFFER_MEMORY_SIZE) {
        size_t chunk = DS_BUFFER_MEMORY_SIZE - refilled;
        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        ASSERT_EQ(AlignUp(chunk), chunk) << "premise: aligned decomposition of the arena";
        uint8_t *p = NULL;
        ASSERT_EQ(ds_malloc(&buf_, reinterpret_cast<void **>(&p), chunk), ERROR_DS_OK)
            << "post-drain refill failed at byte " << refilled << " (seed " << seed << ")";
        refilled += chunk;
    }
}

/* Fixed seeds: deterministic in CI, wide enough to vary the interleavings.
 * To explore a failure locally, add its seed to this list. */
INSTANTIATE_TEST_SUITE_P(Seeds, Monte_Carlo_Tests,
                         ::testing::Values(0x5EED0001u, 0x5EED0002u, 0x5EED0003u,
                                           0xC0FFEE42u));
