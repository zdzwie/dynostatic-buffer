/**
 * @file    dynostatic-buffer.h
 * @brief   Implementation of dynamic allocation of memory in static buffer.
 *
 * @author  Jakub Brzezowski
 * @date    2022-06-14
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "error.h"

#pragma once

/* *INDENT-OFF* */
#ifdef __cplusplus
extern "C" {
#endif
/* *INDENT-ON* */

/**
 * @defgroup ERR_CODES_DS Definitions of dynostatic-buffer error codes.
 * @{
 */
#define ERROR_DS_OK               (0x00) /**< All goes ok. Any error reported.*/
#define ERROR_DS_NO_INIT          (0x01) /**< Dynostatic buffer is not initialized. */
#define ERROR_DS_INVALID_ARG      (0x02) /**< Some given argument is incorrect. */
#define ERROR_DS_ALREADY_INIT     (0x03) /**< Dynostatic buffer was already initialized and cannot be initialized again. */
#define ERROR_DS_NO_MEMORY        (0x04) /**< No free memory to allocate in dynostatic_buffer. */
#define ERROR_DS_NO_ALLOCATORS    (0x05) /**< No free allocators to use. */
#define ERROR_DS_TOO_BIG_CHUNK    (0x06) /**< Demanded size of chunk is bigger than configured max size. */
#define ERROR_DS_MEMORY_OUT_OF_DS (0x07) /**< Pointer given to deallocate is not allocate in dynostatic-buffer. */
#define ERROR_DS_CRITICAL_ERR     (0x08) /**< Critical error detected. */
#define ERROR_DS_PTR_ALLOC_YET    (0x09) /**< Pointer given to function is currently allocated by ds-buffer.*/
/**@}*/

#ifndef DS_BUFFER_MEMORY_SIZE /**< If You not use CMake and KConfig. */
    #define DS_BUFFER_MEMORY_SIZE 1024 /**< Size of buffer prepared for dynostatic-buffer. */
#endif

#ifndef DS_LOG_ENABLE /**< If You not use CMake and KConfig. */
    #define DS_LOG_ENABLE false         /**< Enable or disable logging from dynostatic-buffer. */
#endif

#ifndef DS_MAX_ALLOCATION_COUNT /**< If You not use CMake and KConfig. */
    #define DS_MAX_ALLOCATION_COUNT 10  /**< Set maximal number of allocation which can be made in dynostatic-buffer. */
#endif

#ifndef DS_MAX_ALLOCATION_SIZE /**< If You not use CMake and KConfig. */
    #define DS_MAX_ALLOCATION_SIZE 256  /**< Set maximal number of allocation which can be made in dynostatic-buffer. */
#endif

#if (DS_BUFFER_MEMORY_SIZE == 0) || (DS_MAX_ALLOCATION_COUNT == 0) || (DS_MAX_ALLOCATION_SIZE == 0)
    #error Invalid config!
#endif

#if DS_BUFFER_MEMORY_SIZE < DS_MAX_ALLOCATION_SIZE
    #error To big max allocation count!
#endif

typedef uint16_t ds_err_code_t;      /**< Type of error code used in dynostatic-buffer. */

/**
 * @enum ds_allocator_status_t
 * @brief Current status of allocator.
 */
typedef enum {
    DS_NOT_USED  = 0x00,  /**< Current allocator is not used to describe memory block. */
    DS_FREE      = 0x01,  /**< Current block is used to describe free memory block. */
    DS_ALLOCATED = 0x02   /**< Current block is used to describe allocated memory block. */
} ds_allocator_status_t;

/**
 * @typedef ds_allocator_t
 * @brief Definitions of allocator used to handle information about memory chunks in ds-buffer.
 */
typedef struct __attribute__((packed)) {
    size_t head;  /**< Place in memory, where allocation part is started. */
    size_t size;  /**< Size of current allocated memory chunk. */

    ds_allocator_status_t  allocation_status;  /**< Current allocator is in use by another parts of code.*/
} ds_allocator_t;

/**
 * @typedef dynostatic_buffer_t
 * @brief Structure describe dynostatic buffer, which is using to emulate dynamic allocation without any allocation in heap.
 */
typedef struct __attribute__((packed)) {
    uint8_t memory[DS_BUFFER_MEMORY_SIZE];  /**< Memory declared for buffer. */
    size_t data_head;                       /**< Head of data allocated in buffer. */

    bool initialized;  /**< Dynostatic buffer is initialized. */

    ds_allocator_t allocators[DS_MAX_ALLOCATION_COUNT];  /**< List with structure described possible allocations. */
    size_t used_allocators;                              /**< Number of currently used allocators. */
} dynostatic_buffer_t;

/**
 * @brief Initialize allocation buffer
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 *
 * @retval ERROR_DS_OK Dynostatic-buffer is properly initialized.
 * @retval ERROR_DS_INVALID_ARG Some of given parameter is incorrect.
 */
ds_err_code_t ds_initialize_allocation(dynostatic_buffer_t *p_ds_buffer);

/**
 * @brief Allocate some memory chunk in dynostatic-buffer.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_memory Double pointer, to which will be saved address of allocated memory.
 * @param[in] size Size of block of memory, which will be allocated.
 *
 * @retval ERROR_DS_OK Memory is properly allocated in dynostatic-buffer.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 * @retval ERROR_DS_NO_MEMORY There is not enough free memory to allocate demanded memory chunk.
 * @retval ERROR_DS_NO_ALLOCATORS There is no free allocators. New chunk cannot be add.
 * @retval ERROR_DS_TOO_BIG_CHUNK Demanded size of chunk is bigger than configured max size.
 */
ds_err_code_t ds_malloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size);

/**
 * @brief Deallocate memory block in dynostatic-buffer.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in, out] p_memory Memory block to deallocate.
 *
 * @retval ERROR_DS_OK Memory is properly deallocated.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameter is invalid.
 * @retval ERROR_DS_MEMORY_OUT_OF_DS Pointer given to deallocate is not allocate in dynostatic-buffer.
 */
ds_err_code_t ds_free(dynostatic_buffer_t *p_ds_buffer, void **p_memory);

/**
 * @brief Allocate array of elements of given size and fill it by zeros.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_memory Double pointer, to which will be saved pointer to allocated array.
 * @param[in] len Len of array to allocate.
 * @param[in] size_of_elem Size of one element of array.
 *
 * @retval ERROR_DS_OK Array is properly allocated in dynostatic-buffer.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 * @retval ERROR_DS_NO_MEMORY There is not enough free memory to allocate demanded array.
 * @retval ERROR_DS_NO_ALLOCATORS There is no free allocators. New array cannot be add.
 * @retval ERROR_DS_TOO_BIG_CHUNK Demanded size of array is bigger than configured max size.
 */
ds_err_code_t ds_calloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t len, size_t size_of_elem);

/**
 * @brief Reallocate given memory block to new size. If given pointer is not dynostatic-buffer new memory block will be allocated.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_memory Double pointer, to which will be reallocated. If pointer is not dynostatic-buffer new memory block will be allocated.
 * @param[in] size New size of memory to reallocate.
 *
 * @retval ERROR_DS_OK Memory block is properly reallocated in dynostatic-buffer.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 * @retval ERROR_DS_NO_MEMORY There is not enough free memory to reallocate demanded block.
 * @retval ERROR_DS_TOO_BIG_CHUNK Demanded size of memory block is bigger than configured max size.
 */
ds_err_code_t ds_realloc(dynostatic_buffer_t *p_ds_buffer, const void **p_memory, size_t size);

/**
 * @brief Get usage of memory allocated for dynostatic-buffer in %.
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_memory_usage Pointer to variable where memory usage will be saved.
 *
 * @retval ERROR_DS_OK Memory usage is properly read.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 * @retval ERROR_DS_CRITICAL_ERR More than available memory allocated.
 */
ds_err_code_t ds_get_memory_usage(dynostatic_buffer_t *p_ds_buffer, uint8_t *p_memory_usage);

/**
 * @brief Get size of maximal memory chunk, which could be allocated.
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_max_new_allocation Pointer to variable, where max size will be written.
 *
 * @retval ERROR_DS_OK Max size of new allocation is properly read.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 */
ds_err_code_t ds_get_max_new_allocation_size(dynostatic_buffer_t *p_ds_buffer, size_t *p_max_new_allocation);

/**
 * @brief Check how many new allocation could be made in dynostatic-buffer.
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_free_allocators Pointer to variable, where free allocators count will be written.
 *
 * @retval ERROR_DS_OK Free allocator size is properly read.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 * @retval ERROR_DS_CRITICAL_ERR Used more allocators than is enabled.
 */
ds_err_code_t ds_get_free_allocator_cnt(dynostatic_buffer_t *p_ds_buffer, size_t *p_free_allocators);

/**
 * @brief Deinitialize dynostatic-buffer.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 *
 * @retval ERROR_DS_OK Dynostatic-buffer is properly deinitialized.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 */
ds_err_code_t ds_deinit_allocation(dynostatic_buffer_t *p_ds_buffer);

/* *INDENT-OFF* */
#ifdef __cplusplus
}
#endif
/* *INDENT-ON* */
