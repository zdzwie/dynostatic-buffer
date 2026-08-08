/**
 * @file    example_common.h
 * @brief   Tiny helpers shared by the dynostatic-buffer examples.
 *
 * Header-only on purpose: every example includes it directly, so there is no
 * extra translation unit to compile or link and the build files can keep
 * discovering examples as a plain list of *.c files.
 *
 * This header acts as a facade for the examples: the "IWYU pragma: export"
 * markers below let each example include just this file yet still count as
 * directly including <stdio.h>, <stdlib.h> and the library header for the
 * symbols they use (keeps clang-tidy's misc-include-cleaner happy).
 *
 * @author  Jakub Brzezowski
 */

#ifndef DS_EXAMPLE_COMMON_H
#define DS_EXAMPLE_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#include "dynostatic-buffer.h"

/**
 * @brief Human-readable name for a dynostatic-buffer error code.
 *
 * @param[im] err Dynostatic buffer error to convert.
 *
 * @retval String human-readable representation of dynostatic buffer error.
 */
static inline const char *ds_err_str(ds_err_code_t err)
{
    switch (err) {
        case ERROR_DS_OK:
            return "ERROR_DS_OK";
        case ERROR_DS_NO_INIT:
            return "ERROR_DS_NO_INIT";
        case ERROR_DS_INVALID_ARG:
            return "ERROR_DS_INVALID_ARG";
        case ERROR_DS_ALREADY_INIT:
            return "ERROR_DS_ALREADY_INIT";
        case ERROR_DS_NO_MEMORY:
            return "ERROR_DS_NO_MEMORY";
        case ERROR_DS_NO_ALLOCATORS:
            return "ERROR_DS_NO_ALLOCATORS";
        case ERROR_DS_TOO_BIG_CHUNK:
            return "ERROR_DS_TOO_BIG_CHUNK";
        case ERROR_DS_MEMORY_OUT_OF_DS:
            return "ERROR_DS_MEMORY_OUT_OF_DS";
        case ERROR_DS_CRITICAL_ERR:
            return "ERROR_DS_CRITICAL_ERR";
        case ERROR_DS_ALLOCATOR_NOT_FOUND:
            return "ERROR_DS_ALLOCATOR_NOT_FOUND";
        case ERROR_DS_PTR_ALLOC_YET:
            return "ERROR_DS_PTR_ALLOC_YET";
        default:
            return "ERROR_DS_UNKNOWN";
    }
}

/**
 * Evaluate a dynostatic-buffer call and bail out of main() with a clear
 * message if it does not return ERROR_DS_OK. Use for calls that are expected
 * to succeed; error paths that the example deliberately shows are handled
 * inline instead.
 */
#define DS_EXAMPLE_CHECK(call)                                                                                                                                                                          \
    do {                                                                                                                                                                                                \
        ds_err_code_t ds_example_err_ = (call);                                                                                                                                                         \
        if (ds_example_err_ != ERROR_DS_OK) {                                                                                                                                                           \
            (void)fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__, #call, ds_err_str(ds_example_err_)); /* NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */ \
            return EXIT_FAILURE;                                                                                                                                                                        \
        }                                                                                                                                                                                               \
    } while (0)

#endif /* DS_EXAMPLE_COMMON_H */
