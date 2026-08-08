/**
 * @file    safe_memory_ops.c
 * @brief   Bounds-checked writes with ds_safe_memory_set/ds_safe_memory_copy.
 *
 * Unlike raw memset/memcpy, these helpers verify that the destination really
 * belongs to a live allocation in the instance and that the allocation is
 * large enough for the write. An over-long write is rejected with
 * ERROR_DS_NO_MEMORY instead of corrupting the arena — useful as a guard on
 * embedded targets where a stray write is hard to debug.
 *
 * @author  Jakub Brzezowski
 */

#include <string.h>

#include "example_common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dynostatic-buffer.h"

int main(void)
{
    static dynostatic_buffer_t ds_buffer;
    DS_EXAMPLE_CHECK(ds_initialize_allocation(&ds_buffer));

    /* A 16-byte block to hold a short, NUL-terminated string. */
    char *text = NULL;
    DS_EXAMPLE_CHECK(ds_malloc(&ds_buffer, (void **)&text, 16));

    /* Clear it safely, then copy a string in (length includes the NUL). */
    DS_EXAMPLE_CHECK(ds_safe_memory_set(&ds_buffer, text, 0, 16));

    const char *message = "hello";
    DS_EXAMPLE_CHECK(ds_safe_memory_copy(&ds_buffer, text, message, strlen(message) + 1));
    printf("Copied string: \"%s\"\n", text);

    /* Deliberately overflow the 16-byte block: the guard refuses the write
     * and leaves the buffer untouched. */
    const char *too_long = "this string is definitely longer than sixteen bytes";
    ds_err_code_t err = ds_safe_memory_copy(&ds_buffer, text, too_long, strlen(too_long) + 1);
    printf("Oversized copy rejected as expected: %s\n", ds_err_str(err));
    printf("Original contents intact: \"%s\"\n", text);

    DS_EXAMPLE_CHECK(ds_free(&ds_buffer, (void **)&text));
    DS_EXAMPLE_CHECK(ds_deinit_allocation(&ds_buffer));
    return EXIT_SUCCESS;
}
