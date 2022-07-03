/**
 * @file dynostatic-buffer.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Implementation of dynamic allocation of memory in static buffer.
 * @version 0.1
 * @date 2022-06-14
 *
 */

#include "dynostatic-buffer.h"
#include <stdarg.h>


/**
 * @struct dynostatic_buffer_t
 * @brief Structure describe dynostatic buffer, which is using to emulate dynamic allocation without any allocation in heap.
 */
static struct {
    uint8_t memory[DS_BUFFER_MEMORY_SIZE]; /**< Memory declared for buffer. */

    bool initialized; /**< Dynostatic buffer is initialized. */

    #ifdef DS_LOG_ENABLE
        logger_func_t logger;
    #endif
} dynostatic_buffer;

ds_err_code_t ds_initialize_allocation(logger_func_t logger)
{
    #ifdef DS_LOG_ENABLE
    if (logger == NULL) {
        return EDS_INVALID_PARAMS;
    }
    #endif

    dynostatic_buffer.logger = logger;

    dynostatic_buffer.initialized = true;
    return EDS_OK;
}
