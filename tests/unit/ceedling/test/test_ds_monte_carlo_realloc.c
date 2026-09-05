/**
 * @file test_ds_monte_carlo_realloc.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Model-based randomized storm over malloc/free/realloc. Realloc
 *        outcomes are classified into three buckets (guaranteed fit /
 *        oracle-guaranteed success / externally undecidable), and each
 *        block carries a derivable capacity LOWER BOUND — the storm asserts
 *        only what the public contract guarantees (lesson learned from the
 *        capacity-retention model bug in utests-monte-carlo.cpp).
 *
 * Ceedling/Unity port of tests/unit/utests-monte-carlo-realloc.cpp
 * (Monte_Carlo_Realloc_Tests / Random_Malloc_Free_Realloc_Storm).
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"
#include "utests-random.h"

/** Number of random operations per seed. */
#define kOperations 1000u

/**
 * @struct live_block_t
 * @brief One entry of the shadow model.
 */
typedef struct {
    uint8_t *ptr;     /**< Address of the block. */
    size_t requested; /**< Current requested size (pattern spans this). */
    size_t cap_lower; /**< Derivable lower bound on physical capacity:
                           max(AlignUp(r)) over the block's request history. */
    uint8_t pattern;  /**< Byte pattern written across the whole request. */
} live_block_t;

static dynostatic_buffer_t buf_;

static live_block_t live_[DS_MAX_ALLOCATION_COUNT];
static size_t live_cnt_;
static uint8_t next_pattern_;

void setUp(void)
{
    DsBufferTestSetUp(&buf_);
    (void)memset(live_, 0, sizeof(live_));
    live_cnt_ = 0u;
    next_pattern_ = 1u;
}

void tearDown(void)
{
    DsBufferTestTearDown(&buf_);
}

/**
 * @brief Hand out the next block pattern.
 *
 * @return Pattern byte, never 0.
 */
static uint8_t NextPattern(void)
{
    const uint8_t p = next_pattern_;

    next_pattern_ = (uint8_t)((next_pattern_ % 250u) + 1u);

    return p;
}

/**
 * @brief Drop model entry @p victim, keeping the array compact.
 *
 * @param[in] victim Index to remove.
 */
static void EraseLive(size_t victim)
{
    size_t i;

    for (i = victim; (i + 1u) < live_cnt_; i++) {
        live_[i] = live_[i + 1u];
    }
    live_cnt_--;
}

/**
 * @brief Cross-check the getters against the model after every operation.
 *
 * @param[in] op   Operation index, for failure messages.
 * @param[in] seed Seed in use, for failure messages.
 */
static void AssertGlobalInvariants(size_t op, uint32_t seed)
{
    size_t total_lower = 0u;
    uint8_t usage = 0xFFu;
    size_t cnt = 0xFFu;
    size_t i;

    for (i = 0u; i < live_cnt_; i++) {
        total_lower += live_[i].cap_lower;
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    /* Real capacities may exceed the lower bounds (reuse surplus). */
    TEST_ASSERT_GREATER_OR_EQUAL_UINT_MESSAGE(
        ExpectedUsage(total_lower), usage,
        DS_TEST_MSG("usage below the capacity floor at op %zu (seed 0x%08X)", op, seed));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    /* Block COUNT is capacity-independent: realloc never changes it. */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        (size_t)DS_MAX_ALLOCATION_COUNT - live_cnt_, cnt,
        DS_TEST_MSG("slot count drifted from the model at op %zu (seed 0x%08X)", op, seed));
}

/**
 * @brief Assert a block spanning @p span bytes overlaps no other live block.
 *
 * @param[in] p        Block start.
 * @param[in] span     Bytes it occupies.
 * @param[in] skip_idx Model index to skip (the block itself), or live_cnt_ for none.
 * @param[in] op       Operation index, for failure messages.
 * @param[in] seed     Seed in use, for failure messages.
 */
static void AssertNoOverlapWithLive(const uint8_t *p, size_t span, size_t skip_idx, size_t op,
                                    uint32_t seed)
{
    const uintptr_t a1 = (uintptr_t)p;
    const uintptr_t a2 = a1 + span;
    size_t i;

    for (i = 0u; i < live_cnt_; i++) {
        uintptr_t b1;
        uintptr_t b2;

        if (i == skip_idx) {
            continue;
        }
        b1 = (uintptr_t)live_[i].ptr;
        b2 = b1 + live_[i].cap_lower;

        TEST_ASSERT_TRUE_MESSAGE(
            (a2 <= b1) || (b2 <= a1),
            DS_TEST_MSG("block overlaps a live block at op %zu (seed 0x%08X)", op, seed));
    }
}

/**
 * @brief Assert the first @p span bytes of a block still carry its pattern.
 *
 * @param[in] p_block Model entry to check.
 * @param[in] span    Bytes to check.
 * @param[in] op      Operation index, for failure messages.
 * @param[in] seed    Seed in use, for failure messages.
 */
static void VerifyPattern(const live_block_t *p_block, size_t span, size_t op, uint32_t seed)
{
    size_t i;

    for (i = 0u; i < span; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(
            p_block->pattern, p_block->ptr[i],
            DS_TEST_MSG("corrupted byte %zu at op %zu (seed 0x%08X)", i, op, seed));
    }
}

/**
 * @brief Verify the victim's pattern survived, then free it.
 *
 * @param[in] victim Index into the model.
 * @param[in] op     Operation index, for failure messages.
 * @param[in] seed   Seed in use, for failure messages.
 */
static void VerifyAndFree(size_t victim, size_t op, uint32_t seed)
{
    uint8_t *p = live_[victim].ptr;

    VerifyPattern(&live_[victim], live_[victim].requested, op, seed);

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_free(&buf_, (void **)&p));
    TEST_ASSERT_NULL(p);
    EraseLive(victim);
}

/**
 * @brief Run the whole storm for one seed.
 *
 * @param[in] seed Seed for the deterministic generator.
 */
static void RunStorm(uint32_t seed)
{
    ds_test_rng_t rng;
    size_t op;
    size_t refilled = 0u;
    uint8_t usage = 0xFFu;
    size_t cnt = 0u;
    size_t max_alloc = 0u;

    DsTestRngSeed(&rng, seed);

    for (op = 0u; op < kOperations; op++) {
        const unsigned int roll = (unsigned int)(DsTestRngNext(&rng) % 100u);
        const bool have_live = (live_cnt_ > 0u);

        if (!have_live || (roll < 40u)) {
            /*---------------- malloc ----------------*/
            const size_t req = DsTestRngRange(&rng, 1u, DS_MAX_ALLOCATION_SIZE);
            size_t oracle = 0u;
            uint8_t *p = NULL;
            ds_err_code_t ret;

            TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &oracle));

            ret = ds_malloc(&buf_, (void **)&p, req);

            if (req <= oracle) {
                TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ret,
                                               DS_TEST_MSG("op %zu seed 0x%08X", op, seed));
                TEST_ASSERT_TRUE(IsAligned(p));
                AssertNoOverlapWithLive(p, DS_TEST_ALIGN_UP(req), live_cnt_, op, seed);

                live_[live_cnt_].ptr = p;
                live_[live_cnt_].requested = req;
                live_[live_cnt_].cap_lower = DS_TEST_ALIGN_UP(req);
                live_[live_cnt_].pattern = NextPattern();
                (void)memset(p, live_[live_cnt_].pattern, req);
                live_cnt_++;
            } else {
                TEST_ASSERT_NOT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ret,
                                                   DS_TEST_MSG("op %zu seed 0x%08X", op, seed));
                TEST_ASSERT_NULL(p);
            }
        } else if (roll < 65u) {
            /*---------------- free ----------------*/
            VerifyAndFree(DsTestRngRange(&rng, 0u, live_cnt_ - 1u), op, seed);
        } else if (roll < 70u) {
            /*---------------- realloc(p, 0) == free ----------------*/
            const size_t victim = DsTestRngRange(&rng, 0u, live_cnt_ - 1u);
            void *p = live_[victim].ptr;

            VerifyPattern(&live_[victim], live_[victim].requested, op, seed);

            TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ds_realloc(&buf_, &p, 0u),
                                           DS_TEST_MSG("op %zu seed 0x%08X", op, seed));
            TEST_ASSERT_NULL(p);
            EraseLive(victim);
        } else {
            /*---------------- realloc(p, new_req) ----------------*/
            const size_t victim = DsTestRngRange(&rng, 0u, live_cnt_ - 1u);
            const size_t new_req = DsTestRngRange(&rng, 1u, DS_MAX_ALLOCATION_SIZE);
            live_block_t *p_block = &live_[victim];
            uint8_t *const old_ptr = p_block->ptr;
            const size_t old_req = p_block->requested;
            size_t oracle = 0u;
            void *p = p_block->ptr;
            ds_err_code_t ret;
            bool guaranteed_fit;
            bool oracle_success;

            VerifyPattern(p_block, p_block->requested, op, seed);

            TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &oracle));

            ret = ds_realloc(&buf_, &p, new_req);

            guaranteed_fit = (DS_TEST_ALIGN_UP(new_req) <= p_block->cap_lower);
            oracle_success = (new_req <= oracle);

            if (guaranteed_fit) {
                /* Bucket 1: shrink-or-fit — OK and pointer stability required. */
                TEST_ASSERT_EQUAL_UINT_MESSAGE(ERROR_DS_OK, ret,
                                               DS_TEST_MSG("op %zu seed 0x%08X", op, seed));
                TEST_ASSERT_EQUAL_PTR_MESSAGE(
                    old_ptr, p,
                    DS_TEST_MSG("guaranteed-fit realloc moved the block at op %zu (seed 0x%08X)",
                                op, seed));
            } else if (oracle_success) {
                /* Bucket 2: the move path alone guarantees success. */
                TEST_ASSERT_EQUAL_UINT_MESSAGE(
                    ERROR_DS_OK, ret,
                    DS_TEST_MSG("oracle promised %zu but realloc(%zu) failed at op %zu (seed 0x%08X)",
                                oracle, new_req, op, seed));
            }
            /* Bucket 3 (neither): fit into unseen surplus capacity or the
             * trailing fast path may still succeed — both outcomes legal. */

            if (ret == ERROR_DS_OK) {
                uint8_t *np = (uint8_t *)p;
                const size_t preserved = (old_req < new_req) ? old_req : new_req;
                size_t i;

                TEST_ASSERT_TRUE(IsAligned(np));

                /* Contents preserved up to min(old, new) — regardless of path. */
                for (i = 0u; i < preserved; i++) {
                    TEST_ASSERT_EQUAL_HEX8_MESSAGE(
                        p_block->pattern, np[i],
                        DS_TEST_MSG("content lost at byte %zu, op %zu (seed 0x%08X)", i, op, seed));
                }

                /* Model update — the cap_lower rule keyed on observed movement:
                 * same pointer -> capacity retained: max(old, AlignUp(new));
                 * new pointer  -> fresh history:     AlignUp(new). */
                if (np == old_ptr) {
                    if (DS_TEST_ALIGN_UP(new_req) > p_block->cap_lower) {
                        p_block->cap_lower = DS_TEST_ALIGN_UP(new_req);
                    }
                } else {
                    p_block->cap_lower = DS_TEST_ALIGN_UP(new_req);
                }
                p_block->ptr = np;
                p_block->requested = new_req;
                p_block->pattern = NextPattern();
                (void)memset(np, p_block->pattern, new_req);

                AssertNoOverlapWithLive(np, p_block->cap_lower, victim, op, seed);
            } else {
                /* Failure-intact contract: pointer AND contents untouched. */
                TEST_ASSERT_EQUAL_PTR_MESSAGE(
                    old_ptr, p,
                    DS_TEST_MSG("failed realloc modified the pointer at op %zu (seed 0x%08X)", op,
                                seed));
                VerifyPattern(p_block, old_req, op, seed);
            }
        }

        AssertGlobalInvariants(op, seed);
    }

    /*---------------- drain + no-permanent-fragmentation ----------------*/
    while (live_cnt_ > 0u) {
        VerifyAndFree(DsTestRngRange(&rng, 0u, live_cnt_ - 1u), kOperations, seed);
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, usage, DS_TEST_MSG("seed 0x%08X", seed));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)DS_MAX_ALLOCATION_COUNT, cnt,
                                     DS_TEST_MSG("seed 0x%08X", seed));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &max_alloc));
    TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)DS_MAX_ALLOCATION_SIZE, max_alloc,
                                     DS_TEST_MSG("seed 0x%08X", seed));

    while (refilled < DS_BUFFER_MEMORY_SIZE) {
        size_t chunk = DS_BUFFER_MEMORY_SIZE - refilled;
        uint8_t *p = NULL;

        if (chunk > DS_MAX_ALLOCATION_SIZE) {
            chunk = DS_MAX_ALLOCATION_SIZE;
        }
        TEST_ASSERT_EQUAL_size_t_MESSAGE(chunk, DS_TEST_ALIGN_UP(chunk),
                                         "premise: aligned decomposition of the arena");
        TEST_ASSERT_EQUAL_UINT_MESSAGE(
            ERROR_DS_OK, ds_malloc(&buf_, (void **)&p, chunk),
            DS_TEST_MSG("post-drain refill failed at byte %zu (seed 0x%08X)", refilled, seed));
        refilled += chunk;
    }
}

void test_Monte_Carlo_Realloc_Random_Malloc_Free_Realloc_Storm_Seed_5EED1001(void)
{
    RunStorm(0x5EED1001u);
}

void test_Monte_Carlo_Realloc_Random_Malloc_Free_Realloc_Storm_Seed_5EED1002(void)
{
    RunStorm(0x5EED1002u);
}

void test_Monte_Carlo_Realloc_Random_Malloc_Free_Realloc_Storm_Seed_5EED1003(void)
{
    RunStorm(0x5EED1003u);
}

void test_Monte_Carlo_Realloc_Random_Malloc_Free_Realloc_Storm_Seed_BEEF7777(void)
{
    RunStorm(0xBEEF7777u);
}
