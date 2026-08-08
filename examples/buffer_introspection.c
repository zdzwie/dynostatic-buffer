/**
 * @file    buffer_introspection.c
 * @brief   Inspecting an instance with the read-only query API.
 *
 * dynostatic-buffer exposes three snapshot getters that are handy on
 * constrained targets where you want to reason about capacity before asking
 * for memory:
 *   - ds_get_memory_usage()            arena occupancy as a 0..100 percentage
 *   - ds_get_free_allocator_cnt()      allocator slots not holding a live block
 *   - ds_get_max_new_allocation_size() largest ds_malloc() that would succeed now
 *
 * Each value is a snapshot, valid only until the next mutating call.
 *
 * @author  Jakub Brzezowski
 */

#include "example_common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dynostatic-buffer.h"

static int report(const dynostatic_buffer_t *ds_buffer, const char *when)
{
    uint8_t usage = 0;
    size_t free_slots = 0;
    size_t max_alloc = 0;

    DS_EXAMPLE_CHECK(ds_get_memory_usage(ds_buffer, &usage));
    DS_EXAMPLE_CHECK(ds_get_free_allocator_cnt(ds_buffer, &free_slots));
    DS_EXAMPLE_CHECK(ds_get_max_new_allocation_size(ds_buffer, &max_alloc));

    printf("%-22s usage=%3u%%  free_slots=%2zu  max_new_alloc=%zu\n",
           when, (unsigned)usage, free_slots, max_alloc);
    return EXIT_SUCCESS;
}

int main(void)
{
    static dynostatic_buffer_t ds_buffer;
    DS_EXAMPLE_CHECK(ds_initialize_allocation(&ds_buffer));

    if (report(&ds_buffer, "after init") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    void *block_a = NULL;
    void *block_b = NULL;
    DS_EXAMPLE_CHECK(ds_malloc(&ds_buffer, &block_a, 256));
    DS_EXAMPLE_CHECK(ds_malloc(&ds_buffer, &block_b, 128));
    if (report(&ds_buffer, "after two allocs") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    /* Freeing the trailing block rolls the bump head back, so both the
     * occupancy and the largest satisfiable request recover. */
    DS_EXAMPLE_CHECK(ds_free(&ds_buffer, &block_b));
    if (report(&ds_buffer, "after freeing one") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    DS_EXAMPLE_CHECK(ds_free(&ds_buffer, &block_a));
    DS_EXAMPLE_CHECK(ds_deinit_allocation(&ds_buffer));
    return EXIT_SUCCESS;
}
