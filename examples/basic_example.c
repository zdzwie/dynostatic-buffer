/**
 * @file    basic_example.c
 * @brief   The smallest useful dynostatic-buffer program: initialize an
 *          instance, allocate a block, use it, free it, and tear down.
 *
 * dynostatic-buffer serves dynamic-looking allocations out of a fixed static
 * arena that lives inside the instance itself — no heap, no globals. Prefer
 * static storage duration for the instance: it is comparatively large and is
 * required to be zero-initialized before the first ds_initialize_allocation().
 *
 * @author  Jakub Brzezowski
 */

#include "example_common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dynostatic-buffer.h"

int main(void)
{
    /* Static storage duration: zero-initialized for free (required before the
     * first init) and keeps the sizeable arena off the stack. */
    static dynostatic_buffer_t ds_buffer;

    DS_EXAMPLE_CHECK(ds_initialize_allocation(&ds_buffer));

    /* Allocate room for a handful of integers. The pointer must be NULL going
     * in: ds_malloc refuses to overwrite a pointer to a live block. */
    int *numbers = NULL;
    DS_EXAMPLE_CHECK(ds_malloc(&ds_buffer, (void **)&numbers, 4 * sizeof(int)));

    for (int i = 0; i < 4; ++i) {
        numbers[i] = (i + 1) * 10;
    }

    printf("Allocated 4 ints:");
    for (int i = 0; i < 4; ++i) {
        printf(" %d", numbers[i]);
    }
    printf("\n");

    /* Hand the block back. On success ds_free sets the pointer to NULL, so it
     * is safe to reuse the same variable for the next allocation. */
    DS_EXAMPLE_CHECK(ds_free(&ds_buffer, (void **)&numbers));
    printf("Freed the block (pointer is now %s)\n", numbers == NULL ? "NULL" : "non-NULL");

    DS_EXAMPLE_CHECK(ds_deinit_allocation(&ds_buffer));
    return EXIT_SUCCESS;
}
