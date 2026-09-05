/**
 * @file utests-random.h
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2026
 *
 * @brief Deterministic Mersenne Twister for the model-based storm tests.
 *
 * The GoogleTest suite draws its randomness from std::mt19937; C has no
 * standard equivalent and rand() is implementation-defined, which would make
 * a reported failing seed unreproducible on another machine. This is the
 * plain MT19937 reference algorithm, so a given seed yields the same stream
 * everywhere.
 *
 * @version 1.0
 * @date 2026-09-04
 */
#ifndef UTESTS_RANDOM_H
#define UTESTS_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#define DS_TEST_MT_N 624u /**< Degree of recurrence of MT19937. */

/**
 * @struct ds_test_rng_t
 * @brief State of one MT19937 generator instance.
 */
typedef struct {
    uint32_t state[DS_TEST_MT_N]; /**< Internal state vector. */
    size_t index;                 /**< Read cursor into the state vector. */
} ds_test_rng_t;

/**
 * @brief Seed a generator.
 *
 * @param[out] p_rng Generator to seed.
 * @param[in]  seed  Seed value.
 */
void DsTestRngSeed(ds_test_rng_t *p_rng, uint32_t seed);

/**
 * @brief Draw the next 32-bit value.
 *
 * @param[in,out] p_rng Seeded generator.
 *
 * @return Next value of the stream.
 */
uint32_t DsTestRngNext(ds_test_rng_t *p_rng);

/**
 * @brief Draw a value from the closed range [lo, hi].
 *
 * @param[in,out] p_rng Seeded generator.
 * @param[in]     lo    Lower bound (inclusive).
 * @param[in]     hi    Upper bound (inclusive).
 *
 * @return Value in [lo, hi].
 */
size_t DsTestRngRange(ds_test_rng_t *p_rng, size_t lo, size_t hi);

#endif /* UTESTS_RANDOM_H */
