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
    EDS_OK               = 0x00, /**< Any errors detected. */
    EDS_NO_INIT          = 0x01, /**< Dynostatic buffer is not initialized. */
    EDS_INVALID_PARAMS   = 0x02, /**< Some of parameters given to function is incorrect. */
    EDS_ALREADY_INIT     = 0x03, /**< Dynostatic buffer already initalized*/
    EDS_NO_MEMORY        = 0x04, /**< No free memory to allocate in dynostatic_buffer. */
    EDS_NO_ALLOCATORS    = 0x05, /**< No free allocators to use. */
    EDS_TOO_BIG_CHUNK    = 0x06, /**< Demanded size of chunk is bigger than configured max size. */
    EDS_MEMORY_OUT_OF_DS = 0x07, /**< Pointer given to deallocate is not allocate in dynostatic-buffer. */
    //TODO: Finish error codes
} ds_err_code_t;

/**
 * @brief Declaration of function pointer using by logger function in dynostatic-buffer, when logging is enabled.
 * @note Logging could be enabled in dynostatic-buffer-defs.h
 */
typedef void (*logger_func_t)(const char *, size_t);

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

/**
 * @brief Allocate some memory chunk in dynostatic-buffer.
 *
 * @param[out] p_memory Double pointer, to which will be saved address of allocated memory.
 * @param[in] size Size of block of memory, which will be allocated.
 *
 * @retval EDS_OK Memory is properly allocated in dynostatic-buffer.
 * @retval EDS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval EDS_INVALID_PARAMS Given parameters are invalid.
 * @retval EDS_NO_MEMORY There is not enough free memory to allocate demanded memory chunk.
 * @retval EDS_NO_ALLOCATORS There is no free allocators. New chunk cannot be add.
 * @retval EDS_TOO_BIG_CHUNK Demanded size of chunk is bigger than configured max size.
 */
ds_err_code_t ds_malloc(void **p_memory, size_t size);

/**
 * @brief Deallocate memory block in dynostatic-buffer.
 *
 * @param[in, out] p_memory Memory block to deallocate.
 *
 * @retval EDS_OK Memory is properly deallocated.
 * @retval EDS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval EDS_INVALID_PARAMS Given parameter is invalid.
 * @retval EDS_MEMORY_OUT_OF_DS Pointer given to deallocate is not allocate in dynostatic-buffer.
 */
ds_err_code_t ds_free(void **p_memory);

/**
 * @brief Allocate array of elements of given size and fill it by zeros.
 *
 * @param[out] p_memory Double pointer, to which will be saved pointer to allocated array.
 * @param[in] len Len of array to allocate.
 * @param[in] size_of_elem Size of one element of array.
 *
 * @retval EDS_OK Array is properly allocated in dynostatic-buffer.
 * @retval EDS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval EDS_INVALID_PARAMS Given parameters are invalid.
 * @retval EDS_NO_MEMORY There is not enough free memory to allocate demanded array.
 * @retval EDS_NO_ALLOCATORS There is no free allocators. New array cannot be add.
 * @retval EDS_TOO_BIG_CHUNK Demanded size of array is bigger than configured max size.
 */
ds_err_code_t ds_calloc(void **p_memory, size_t len, size_t size_of_elem);

/**
 * @brief Reallocate given memory block to new size. If given pointer is not dynostatic-buffer new memory block will be allocated.
 *
 * @param[out] p_memory Double pointer, to which will be reallocated. If pointer is not dynostatic-buffer new memory block will be allocated.
 * @param[in] size New size of memory to reallocate.
 *
 * @retval EDS_OK Memory block is properly reallocated in dynostatic-buffer.
 * @retval EDS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval EDS_INVALID_PARAMS Given parameters are invalid.
 * @retval EDS_NO_MEMORY There is not enough free memory to reallocate demanded block.
 * @retval EDS_TOO_BIG_CHUNK Demanded size of memory block is bigger than configured max size.
 */
ds_err_code_t ds_realloc(void **p_memory, size_t size);
