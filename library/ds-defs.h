/**
 * @file ds-defs.h
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Declaration of all defines used to configure dynostatic-buffer.
 * @version 0.1
 * @date 2022-06-14
 *
 */

#include <stdbool.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DS_BUFFER_MEMORY_SIZE /**< If You not use CMake. */
    #define DS_BUFFER_MEMORY_SIZE 1024 /**< Size of buffer prepared for dynostatic-buffer. */
#endif

#ifndef DS_LOG_ENABLE /**< If You not use CMake. */
    #define DS_LOG_ENABLE false         /**< Enable or disable logging from dynostatic-buffer. */
#endif

#ifndef DS_MAX_ALLOCATION_COUNT /**< If You not use CMake. */
    #define DS_MAX_ALLOCATION_COUNT 10  /**< Set maximal number of allocation which can be made in dynostatic-buffer. */
#endif

#ifndef DS_MAX_ALLOCATION_SIZE /**< If You not use CMake. */
    #define DS_MAX_ALLOCATION_SIZE 256  /**< Set maximal number of allocation which can be made in dynostatic-buffer. */
#endif

#if (DS_BUFFER_MEMORY_SIZE == 0) || (DS_MAX_ALLOCATION_COUNT == 0) || (DS_MAX_ALLOCATION_SIZE == 0)
    #error Invalid config!
#endif

#if DS_BUFFER_MEMORY_SIZE < DS_MAX_ALLOCATION_SIZE
    #error To big max allocation count!
#endif

#ifdef __cplusplus
}
#endif
