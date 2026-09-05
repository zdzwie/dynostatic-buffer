/**
 * @file utests-random.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief MT19937 reference implementation used by the storm tests.
 * @version 1.0
 * @date 2026-09-04
 */

#include "utests-random.h"

#define DS_TEST_MT_M          397u
#define DS_TEST_MT_MATRIX_A   0x9908B0DFuL
#define DS_TEST_MT_UPPER_MASK 0x80000000uL
#define DS_TEST_MT_LOWER_MASK 0x7FFFFFFFuL

void DsTestRngSeed(ds_test_rng_t *p_rng, uint32_t seed)
{
    size_t i;

    p_rng->state[0] = seed;
    for (i = 1u; i < DS_TEST_MT_N; i++) {
        p_rng->state[i] = (uint32_t)(1812433253uL * (p_rng->state[i - 1u] ^ (p_rng->state[i - 1u] >> 30)) + i);
    }
    p_rng->index = DS_TEST_MT_N;
}

/**
 * @brief Regenerate the whole state vector once the cursor runs out.
 *
 * @param[in,out] p_rng Generator whose state is exhausted.
 */
static void DsTestRngTwist(ds_test_rng_t *p_rng)
{
    static const uint32_t mag01[2] = { 0x0uL, DS_TEST_MT_MATRIX_A };
    size_t i;
    uint32_t y;

    for (i = 0u; i < (DS_TEST_MT_N - DS_TEST_MT_M); i++) {
        y = (p_rng->state[i] & DS_TEST_MT_UPPER_MASK) | (p_rng->state[i + 1u] & DS_TEST_MT_LOWER_MASK);
        p_rng->state[i] = p_rng->state[i + DS_TEST_MT_M] ^ (y >> 1) ^ mag01[y & 0x1uL];
    }
    for (; i < (DS_TEST_MT_N - 1u); i++) {
        y = (p_rng->state[i] & DS_TEST_MT_UPPER_MASK) | (p_rng->state[i + 1u] & DS_TEST_MT_LOWER_MASK);
        p_rng->state[i] = p_rng->state[i + DS_TEST_MT_M - DS_TEST_MT_N] ^ (y >> 1) ^ mag01[y & 0x1uL];
    }
    y = (p_rng->state[DS_TEST_MT_N - 1u] & DS_TEST_MT_UPPER_MASK) | (p_rng->state[0] & DS_TEST_MT_LOWER_MASK);
    p_rng->state[DS_TEST_MT_N - 1u] = p_rng->state[DS_TEST_MT_M - 1u] ^ (y >> 1) ^ mag01[y & 0x1uL];

    p_rng->index = 0u;
}

uint32_t DsTestRngNext(ds_test_rng_t *p_rng)
{
    uint32_t y;

    if (p_rng->index >= DS_TEST_MT_N) {
        DsTestRngTwist(p_rng);
    }

    y = p_rng->state[p_rng->index];
    p_rng->index++;

    /* Tempering. */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9D2C5680uL;
    y ^= (y << 15) & 0xEFC60000uL;
    y ^= (y >> 18);

    return y;
}

size_t DsTestRngRange(ds_test_rng_t *p_rng, size_t lo, size_t hi)
{
    const size_t span = (hi - lo) + 1u;

    return lo + ((size_t)DsTestRngNext(p_rng) % span);
}
