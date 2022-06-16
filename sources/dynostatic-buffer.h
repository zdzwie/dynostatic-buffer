/**
 * @file dynostatic-buffer.h
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Implementation of dynamic allocation of memory in static buffer.
 * @version 0.1
 * @date 2022-06-14
 *
 */

#include <stdint.h>
#include <stdbool.h>

#include "ds-defs.h"

#pragma once

/**
 * @def DS_VAL_ERR_AND_RET
 * @brief Check correctness of function return value. In case of error return it.
 *
 * @param[in] err Return value to validate.
 */
#define DS_VAL_ERR_AND_RET(err) do { \
    if ((ds_err_code_t)err != EDS_OK) { \
        return err; \
    } \
} while(0)

/**
 * @enum ds_err_codes
 * @brief All error codes, which can be returned by dynostatic-buffer memory allocation.
 */
typedef enum ds_err_codes {
    EDS_OK             = 0x00, /**< Any errors detected. */
    EDS_NO_INIT        = 0x01, /**< Dynostatic buffer is not initialized. */
    EDS_INVALID_PARAMS = 0x02, /**< Some of parameters given to function is incorrect. */
    //TODO: Finish error codes
} ds_err_code_t;

/**
 * @brief Declaration of function pointer using by logger function in dynostatic-buffer, when logging is enabled.
 * @note Logging could be enabled in dynostatic-buffer-defs.h
 */
typedef void (*logger_func_t)(char *, size_t);

/**
 * @brief Initialize allocation buffer
 *
 * @param[in] logger Pointer to function, which will be used to print log from dynostatic-buffer.
 *            If logging is disable this parameter could be get by NULL, because won't be used. In
 *            other cases given NULL lead to returning error and break process of initialization.
 *
 * @retval EDS_OK Dynostatic-buffer is properly initialized.
 * @retval EDS_INVALID_PARAMS Some of given parameter is incorrect.
 */
ds_err_code_t ds_initialize_allocation(logger_func_t logger);
