/**
 * @file    array_calloc_realloc.c
 * @brief   Growing a zero-initialized array with ds_calloc() and ds_realloc().
 *
 * Shows the two array-oriented entry points:
 *   - ds_calloc() allocates and zero-fills (independently of DS_ZERO_ON_FREE),
 *     with an overflow-checked element count * element size.
 *   - ds_realloc() resizes while preserving existing contents. Growing the
 *     most recently placed block is done in place, so the pointer typically
 *     does not move — but always assign the result back, because in the
 *     general case the block is relocated.
 *
 * @author  Jakub Brzezowski
 */

#include "example_common.h"

#include <stdlib.h>
#include <stdio.h>

#include "dynostatic-buffer.h"

static void print_array(const char *label, const int *values, size_t count)
{
    printf("%s (%zu):", label, count);
    for (size_t i = 0; i < count; ++i) {
        printf(" %d", values[i]);
    }
    printf("\n");
}

int main(void)
{
    static dynostatic_buffer_t ds_buffer;
    DS_EXAMPLE_CHECK(ds_initialize_allocation(&ds_buffer));

    /* Zero-initialized array of 4 ints. */
    int *values = NULL;
    DS_EXAMPLE_CHECK(ds_calloc(&ds_buffer, (void **)&values, 4, sizeof(int)));
    print_array("After ds_calloc, all zero", values, 4);

    for (int i = 0; i < 4; ++i) {
        values[i] = i + 1;
    }
    print_array("After filling", values, 4);

    /* Grow to 8 ints. This is the most recently allocated block, so the grow
     * happens in place and the first 4 values are preserved. The bytes beyond
     * the old size are indeterminate, so initialize them explicitly. */
    DS_EXAMPLE_CHECK(ds_realloc(&ds_buffer, (void **)&values, 8 * sizeof(int)));
    for (int i = 4; i < 8; ++i) {
        values[i] = i + 1;
    }
    print_array("After ds_realloc grow to 8", values, 8);

    DS_EXAMPLE_CHECK(ds_free(&ds_buffer, (void **)&values));
    DS_EXAMPLE_CHECK(ds_deinit_allocation(&ds_buffer));
    return EXIT_SUCCESS;
}
