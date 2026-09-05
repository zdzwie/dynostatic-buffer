/**
 * @file test_ds_monte_carlo_malloc.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Model-based randomized stress test: a deterministic storm of
 *        malloc/free operations checked against a shadow model, with
 *        ds_get_max_new_allocation_size() as the success oracle and a
 *        no-permanent-fragmentation end-state assertion.
 *
 * Ceedling/Unity port of tests/unit/utests-monte-carlo-malloc.cpp
 * (Monte_Carlo_Tests / Random_Alloc_Free_Storm). std::vector becomes a
 * fixed array — the model can never hold more than DS_MAX_ALLOCATION_COUNT
 * live blocks — and std::mt19937 becomes the MT19937 in utests-random.c, so
 * a reported seed still reproduces exactly.
 *
 * @version 1.0
 * @date 2026-09-04
 */
#include "dynostatic-buffer.h"
#include "utests-common.h"
#include "utests-random.h"

/** Number of random operations per seed. */
#define kOperations        1000u
/** malloc vs free mix while blocks are live. */
#define kMallocBiasPercent 55u

/**
 * @struct live_block_t
 * @brief One entry of the shadow model: a block the test believes is live.
 */
typedef struct {
    uint8_t *ptr;     /**< Address handed out by ds_malloc. */
    size_t requested; /**< Size requested for it. */
    uint8_t pattern;  /**< Byte pattern written across the whole request. */
} live_block_t;

static dynostatic_buffer_t buf_;

/** Shadow model: what the test believes is currently allocated. */
static live_block_t live_[DS_MAX_ALLOCATION_COUNT];
static size_t live_cnt_;
/** Never 0: distinguishes user data from zeroed memory. */
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
    size_t total_aligned = 0u;
    uint8_t usage = 0xFFu;
    size_t cnt = 0xFFu;
    size_t i;

    for (i = 0u; i < live_cnt_; i++) {
        total_aligned += DS_TEST_ALIGN_UP(live_[i].requested);
    }

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    /* Reused blocks RETAIN their original capacity (see ds_allocator_t), so
     * real usage may exceed the sum of aligned requests — the model can
     * assert a LOWER BOUND per-op; exact equality holds only at drain. */
    TEST_ASSERT_GREATER_OR_EQUAL_UINT_MESSAGE(
        ExpectedUsage(total_aligned), usage,
        DS_TEST_MSG("usage below the aligned-requests floor at op %zu (seed 0x%08X)", op, seed));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        (size_t)DS_MAX_ALLOCATION_COUNT - live_cnt_, cnt,
        DS_TEST_MSG("slot count drifted from the model at op %zu (seed 0x%08X)", op, seed));
}

/**
 * @brief Assert a freshly delivered block overlaps none of the live ones.
 *
 * @param[in] p         Block start.
 * @param[in] requested Size requested for it.
 * @param[in] op        Operation index, for failure messages.
 * @param[in] seed      Seed in use, for failure messages.
 */
static void AssertNoOverlapWithLive(const uint8_t *p, size_t requested, size_t op, uint32_t seed)
{
    const uintptr_t a1 = (uintptr_t)p;
    const uintptr_t a2 = a1 + DS_TEST_ALIGN_UP(requested);
    size_t i;

    for (i = 0u; i < live_cnt_; i++) {
        const uintptr_t b1 = (uintptr_t)live_[i].ptr;
        const uintptr_t b2 = b1 + DS_TEST_ALIGN_UP(live_[i].requested);

        TEST_ASSERT_TRUE_MESSAGE(
            (a2 <= b1) || (b2 <= a1),
            DS_TEST_MSG("new block overlaps a live block at op %zu (seed 0x%08X)", op, seed));
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
    size_t i;

    /* Cross-corruption detector: the pattern written at allocation time
     * must survive every operation performed since. */
    for (i = 0u; i < live_[victim].requested; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(
            live_[victim].pattern, live_[victim].ptr[i],
            DS_TEST_MSG("block corrupted at byte %zu, op %zu (seed 0x%08X)", i, op, seed));
    }

    TEST_ASSERT_EQUAL_UINT_MESSAGE(
        ERROR_DS_OK, ds_free(&buf_, (void **)&p),
        DS_TEST_MSG("free of a model-live block failed at op %zu (seed 0x%08X)", op, seed));
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
        const bool do_malloc = (live_cnt_ == 0u) || ((DsTestRngNext(&rng) % 100u) < kMallocBiasPercent);

        if (do_malloc) {
            const size_t req = DsTestRngRange(&rng, 1u, DS_MAX_ALLOCATION_SIZE);
            size_t oracle = 0u;
            uint8_t *p = NULL;
            ds_err_code_t ret;

            /* Oracle: the getter's tight contract decides the expected
             * outcome BEFORE the call — transient fragmentation is allowed
             * exactly when the oracle says so, and never otherwise. */
            TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &oracle));

            ret = ds_malloc(&buf_, (void **)&p, req);

            if (req <= oracle) {
                TEST_ASSERT_EQUAL_UINT_MESSAGE(
                    ERROR_DS_OK, ret,
                    DS_TEST_MSG("oracle promised %zu but malloc(%zu) failed at op %zu (seed 0x%08X)",
                                oracle, req, op, seed));
                TEST_ASSERT_TRUE(IsAligned(p));
                AssertNoOverlapWithLive(p, req, op, seed);

#if DS_ZERO_ON_FREE
                /* Under the zero-on-free policy every delivered block —
                 * fresh or reused — must arrive clean. */
                {
                    size_t i;
                    for (i = 0u; i < req; i++) {
                        TEST_ASSERT_EQUAL_HEX8_MESSAGE(
                            0u, p[i],
                            DS_TEST_MSG("dirty byte %zu in a delivered block, op %zu (seed 0x%08X)",
                                        i, op, seed));
                    }
                }
#endif

                live_[live_cnt_].ptr = p;
                live_[live_cnt_].requested = req;
                live_[live_cnt_].pattern = NextPattern();
                (void)memset(p, live_[live_cnt_].pattern, req);
                live_cnt_++;
            } else {
                TEST_ASSERT_NOT_EQUAL_UINT_MESSAGE(
                    ERROR_DS_OK, ret,
                    DS_TEST_MSG("malloc(%zu) succeeded past the oracle bound %zu at op %zu (seed 0x%08X)",
                                req, oracle, op, seed));
                TEST_ASSERT_NULL_MESSAGE(p, "failed malloc modified the out-pointer");
            }
        } else {
            VerifyAndFree(DsTestRngRange(&rng, 0u, live_cnt_ - 1u), op, seed);
        }

        AssertGlobalInvariants(op, seed);
    }

    /* Drain in random order: park/cascade interleavings at their messiest. */
    while (live_cnt_ > 0u) {
        VerifyAndFree(DsTestRngRange(&rng, 0u, live_cnt_ - 1u), kOperations, seed);
    }

    /* NO PERMANENT FRAGMENTATION: after a full drain the instance must be
     * indistinguishable from a freshly initialized one. */
    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_memory_usage(&buf_, &usage));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, usage,
                                    DS_TEST_MSG("usage did not return to zero (seed 0x%08X)", seed));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_free_allocator_cnt(&buf_, &cnt));
    TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)DS_MAX_ALLOCATION_COUNT, cnt,
                                     DS_TEST_MSG("allocator slots leaked (seed 0x%08X)", seed));

    TEST_ASSERT_EQUAL_UINT(ERROR_DS_OK, ds_get_max_new_allocation_size(&buf_, &max_alloc));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        (size_t)DS_MAX_ALLOCATION_SIZE, max_alloc,
        DS_TEST_MSG("post-drain capacity below pristine (seed 0x%08X)", seed));

    /* Behavioural proof: the WHOLE arena is allocatable again. */
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

/* Fixed seeds: deterministic in CI, wide enough to vary the interleavings.
 * To explore a failure locally, add a test for its seed here. */

void test_Monte_Carlo_Random_Alloc_Free_Storm_Seed_5EED0001(void)
{
    RunStorm(0x5EED0001u);
}

void test_Monte_Carlo_Random_Alloc_Free_Storm_Seed_5EED0002(void)
{
    RunStorm(0x5EED0002u);
}

void test_Monte_Carlo_Random_Alloc_Free_Storm_Seed_5EED0003(void)
{
    RunStorm(0x5EED0003u);
}

void test_Monte_Carlo_Random_Alloc_Free_Storm_Seed_C0FFEE42(void)
{
    RunStorm(0xC0FFEE42u);
}
