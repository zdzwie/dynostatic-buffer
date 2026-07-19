/**
 * @file utests-monte-carlo-realloc.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Model-based randomized storm over malloc/free/realloc. Realloc
 *        outcomes are classified into three buckets (guaranteed fit /
 *        oracle-guaranteed success / externally undecidable), and each
 *        block carries a derivable capacity LOWER BOUND — the storm asserts
 *        only what the public contract guarantees (lesson learned from the
 *        capacity-retention model bug in utests-monte-carlo.cpp).
 * @version 0.1
 * @date 2026-07-14
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
} // namespace

class Monte_Carlo_Realloc_Tests : public DsBufferTest,
                                  public ::testing::WithParamInterface<uint32_t> {
  protected:
    struct LiveBlock {
        uint8_t *ptr;
        size_t requested; /* current requested size (pattern spans this) */
        size_t cap_lower; /* derivable lower bound on physical capacity:
                              max(AlignUp(r)) over the block's request history */
        uint8_t pattern;
    };

    std::vector<LiveBlock> live_;
    uint8_t next_pattern_ = 1u;

    uint8_t NextPattern()
    {
        const uint8_t p = next_pattern_;
        next_pattern_ = static_cast<uint8_t>((next_pattern_ % 250u) + 1u);
        return p;
    }

    void AssertGlobalInvariants(size_t op, uint32_t seed)
    {
        const size_t total_lower = std::accumulate(
            live_.cbegin(), live_.cend(), size_t{ 0 },
            [](size_t acc, const LiveBlock &b) { return acc + b.cap_lower; });

        uint8_t usage = 0xFFu;
        ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
        /* Real capacities may exceed the lower bounds (reuse surplus). */
        ASSERT_GE(static_cast<unsigned>(usage),
                  static_cast<unsigned>(ExpectedUsage(total_lower)))
            << "usage below the capacity floor at op " << op << " (seed " << seed << ")";

        size_t cnt = 0xFFu;
        ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
        /* Block COUNT is capacity-independent: realloc never changes it. */
        ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT) - live_.size())
            << "slot count drifted from the model at op " << op << " (seed " << seed << ")";
    }

    void AssertNoOverlapWithLive(const uint8_t *p, size_t span, size_t skip_idx,
                                 size_t op, uint32_t seed)
    {
        const uintptr_t a1 = reinterpret_cast<uintptr_t>(p);
        const uintptr_t a2 = a1 + span;
        for (size_t i = 0; i < live_.size(); i++) {
            if (i == skip_idx) {
                continue;
            }
            const uintptr_t b1 = reinterpret_cast<uintptr_t>(live_[i].ptr);
            const uintptr_t b2 = b1 + live_[i].cap_lower;
            ASSERT_TRUE((a2 <= b1) || (b2 <= a1))
                << "block overlaps a live block at op " << op << " (seed " << seed << ")";
        }
    }

    void VerifyPattern(const LiveBlock &b, size_t span, size_t op, uint32_t seed)
    {
        for (size_t i = 0; i < span; i++) {
            ASSERT_EQ(b.ptr[i], b.pattern)
                << "corrupted byte " << i << " at op " << op << " (seed " << seed << ")";
        }
    }

    void VerifyAndFree(size_t victim, size_t op, uint32_t seed)
    {
        ASSERT_NO_FATAL_FAILURE(VerifyPattern(live_[victim], live_[victim].requested, op, seed));

        uint8_t *p = live_[victim].ptr;
        ASSERT_EQ(ds_free(&buf_, reinterpret_cast<void **>(&p)), ERROR_DS_OK);
        ASSERT_EQ(p, nullptr);
        live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(victim));
    }
};

TEST_P(Monte_Carlo_Realloc_Tests, Random_Malloc_Free_Realloc_Storm)
{
    const uint32_t seed = GetParam();
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> size_dist(1u, DS_MAX_ALLOCATION_SIZE);

    for (size_t op = 0u; op < kOperations; op++) {
        const unsigned roll = rng() % 100u;
        const bool have_live = !live_.empty();

        if (!have_live || (roll < 40u)) {
            /*---------------- malloc ----------------*/
            const size_t req = size_dist(rng);
            size_t oracle = 0u;
            ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &oracle), ERROR_DS_OK);

            uint8_t *p = NULL;
            const ds_err_code_t ret = ds_malloc(&buf_, reinterpret_cast<void **>(&p), req);

            if (req <= oracle) {
                ASSERT_EQ(ret, ERROR_DS_OK) << "op " << op << " seed " << seed;
                ASSERT_TRUE(IsAligned(p));
                ASSERT_NO_FATAL_FAILURE(
                    AssertNoOverlapWithLive(p, AlignUp(req), live_.size(), op, seed));
                const uint8_t pattern = NextPattern();
                std::memset(p, pattern, req);
                live_.push_back(LiveBlock{ p, req, AlignUp(req), pattern });
            } else {
                ASSERT_NE(ret, ERROR_DS_OK) << "op " << op << " seed " << seed;
                ASSERT_EQ(p, nullptr);
            }
        } else if (roll < 65u) {
            /*---------------- free ----------------*/
            std::uniform_int_distribution<size_t> victim_dist(0u, live_.size() - 1u);
            ASSERT_NO_FATAL_FAILURE(VerifyAndFree(victim_dist(rng), op, seed));
        } else if (roll < 70u) {
            /*---------------- realloc(p, 0) == free ----------------*/
            std::uniform_int_distribution<size_t> victim_dist(0u, live_.size() - 1u);
            const size_t victim = victim_dist(rng);
            ASSERT_NO_FATAL_FAILURE(VerifyPattern(live_[victim], live_[victim].requested, op, seed));

            void *p = live_[victim].ptr;
            ASSERT_EQ(ds_realloc(&buf_, &p, 0u), ERROR_DS_OK) << "op " << op << " seed " << seed;
            ASSERT_EQ(p, nullptr);
            live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(victim));
        } else {
            /*---------------- realloc(p, new_req) ----------------*/
            std::uniform_int_distribution<size_t> victim_dist(0u, live_.size() - 1u);
            const size_t victim = victim_dist(rng);
            const size_t new_req = size_dist(rng);
            LiveBlock &b = live_[victim];

            ASSERT_NO_FATAL_FAILURE(VerifyPattern(b, b.requested, op, seed));

            size_t oracle = 0u;
            ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &oracle), ERROR_DS_OK);

            uint8_t *const old_ptr = b.ptr;
            const size_t old_req = b.requested;
            void *p = b.ptr;
            const ds_err_code_t ret = ds_realloc(&buf_, &p, new_req);

            const bool guaranteed_fit = (AlignUp(new_req) <= b.cap_lower);
            const bool oracle_success = (new_req <= oracle);

            if (guaranteed_fit) {
                /* Bucket 1: shrink-or-fit — OK and pointer stability required. */
                ASSERT_EQ(ret, ERROR_DS_OK) << "op " << op << " seed " << seed;
                ASSERT_EQ(p, old_ptr)
                    << "guaranteed-fit realloc moved the block at op " << op
                    << " (seed " << seed << ")";
            } else if (oracle_success) {
                /* Bucket 2: the move path alone guarantees success. */
                ASSERT_EQ(ret, ERROR_DS_OK)
                    << "oracle promised " << oracle << " but realloc(" << new_req
                    << ") failed at op " << op << " (seed " << seed << ")";
            }
            /* Bucket 3 (neither): fit into unseen surplus capacity or the
             * trailing fast path may still succeed — both outcomes legal. */

            if (ret == ERROR_DS_OK) {
                uint8_t *np = static_cast<uint8_t *>(p);
                ASSERT_TRUE(IsAligned(np));

                /* Contents preserved up to min(old, new) — regardless of path. */
                const size_t preserved = (old_req < new_req) ? old_req : new_req;
                for (size_t i = 0; i < preserved; i++) {
                    ASSERT_EQ(np[i], b.pattern)
                        << "content lost at byte " << i << ", op " << op
                        << " (seed " << seed << ")";
                }

                /* Model update — the cap_lower rule keyed on observed movement:
                 * same pointer -> capacity retained: max(old, AlignUp(new));
                 * new pointer  -> fresh history:     AlignUp(new). */
                b.cap_lower = (np == old_ptr)
                                ? std::max(b.cap_lower, AlignUp(new_req))
                                : AlignUp(new_req);
                b.ptr = np;
                b.requested = new_req;
                b.pattern = NextPattern();
                std::memset(np, b.pattern, new_req);

                ASSERT_NO_FATAL_FAILURE(
                    AssertNoOverlapWithLive(np, b.cap_lower, victim, op, seed));
            } else {
                /* Failure-intact contract: pointer AND contents untouched. */
                ASSERT_EQ(p, old_ptr)
                    << "failed realloc modified the pointer at op " << op
                    << " (seed " << seed << ")";
                ASSERT_NO_FATAL_FAILURE(VerifyPattern(b, old_req, op, seed));
            }
        }

        ASSERT_NO_FATAL_FAILURE(AssertGlobalInvariants(op, seed));
    }

    /*---------------- drain + no-permanent-fragmentation ----------------*/
    std::shuffle(live_.begin(), live_.end(), rng);
    while (!live_.empty()) {
        ASSERT_NO_FATAL_FAILURE(VerifyAndFree(live_.size() - 1u, kOperations, seed));
    }

    uint8_t usage = 0xFFu;
    ASSERT_EQ(ds_get_memory_usage(&buf_, &usage), ERROR_DS_OK);
    ASSERT_EQ(static_cast<unsigned>(usage), 0u) << "seed " << seed;

    size_t cnt = 0u;
    ASSERT_EQ(ds_get_free_allocator_cnt(&buf_, &cnt), ERROR_DS_OK);
    ASSERT_EQ(cnt, static_cast<size_t>(DS_MAX_ALLOCATION_COUNT)) << "seed " << seed;

    size_t max_alloc = 0u;
    ASSERT_EQ(ds_get_max_new_allocation_size(&buf_, &max_alloc), ERROR_DS_OK);
    ASSERT_EQ(max_alloc, static_cast<size_t>(DS_MAX_ALLOCATION_SIZE)) << "seed " << seed;

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

INSTANTIATE_TEST_SUITE_P(Seeds, Monte_Carlo_Realloc_Tests,
                         ::testing::Values(0x5EED1001u, 0x5EED1002u, 0x5EED1003u,
                                           0xBEEF7777u));
