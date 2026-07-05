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
#include <stdalign.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*---------Macros-And-Defines---------*/

/**
 * @defgroup ERR_CODES_DS Definitions of dynostatic-buffer error codes.
 * @{
 */
#define ERROR_DS_OK               ((ds_err_code_t)(0x00u)) /**< All goes ok. Any error reported.*/
#define ERROR_DS_NO_INIT          ((ds_err_code_t)(0x01u)) /**< Dynostatic buffer is not initialized. */
#define ERROR_DS_INVALID_ARG      ((ds_err_code_t)(0x02u)) /**< Some given argument is incorrect. */
#define ERROR_DS_ALREADY_INIT     ((ds_err_code_t)(0x03u)) /**< Dynostatic buffer was already initialized and cannot be initialized again. */
#define ERROR_DS_NO_MEMORY        ((ds_err_code_t)(0x04u)) /**< No free memory to allocate in dynostatic_buffer. */
#define ERROR_DS_NO_ALLOCATORS    ((ds_err_code_t)(0x05u)) /**< No free allocators to use. */
#define ERROR_DS_TOO_BIG_CHUNK    ((ds_err_code_t)(0x06u)) /**< Demanded size of chunk is bigger than configured max size. */
#define ERROR_DS_MEMORY_OUT_OF_DS ((ds_err_code_t)(0x07u)) /**< Pointer given to deallocate is not allocate in dynostatic-buffer. */
#define ERROR_DS_CRITICAL_ERR     ((ds_err_code_t)(0x08u)) /**< Critical error detected. */
#define ERROR_DS_PTR_ALLOC_YET    ((ds_err_code_t)(0x09u)) /**< Pointer given to function is currently allocated by ds-buffer.*/
/**@}*/

#ifndef DS_BUFFER_MEMORY_SIZE           /**< If You not use CMake and KConfig. */
    #define DS_BUFFER_MEMORY_SIZE 1024u /**< Size of buffer prepared for dynostatic-buffer. */
#endif

#ifndef DS_LOG_ENABLE           /**< If You not use CMake and KConfig. */
    #define DS_LOG_ENABLE false /**< Enable or disable logging from dynostatic-buffer. */
#endif

#ifndef DS_MAX_ALLOCATION_COUNT         /**< If You not use CMake and KConfig. */
    #define DS_MAX_ALLOCATION_COUNT 10u /**< Set maximal number of allocation which can be made in dynostatic-buffer. */
#endif

#ifndef DS_MAX_ALLOCATION_SIZE          /**< If You not use CMake and KConfig. */
    #define DS_MAX_ALLOCATION_SIZE 256u /**< Set maximal number of allocation which can be made in dynostatic-buffer. */
#endif

#ifndef DS_ALIGNMENT
    #define DS_ALIGNMENT (4u) /**< Alignment for memory allocations. */
#endif

#ifdef __cplusplus
    #define DS_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define DS_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

DS_STATIC_ASSERT((DS_ALIGNMENT & (DS_ALIGNMENT - 1u)) == 0u, "DS_ALIGNMENT must be a power of two");
DS_STATIC_ASSERT(DS_ALIGNMENT >= alignof(uint32_t), "DS_ALIGNMENT must at least satisfy 32-bit types");
DS_STATIC_ASSERT(DS_ALIGNMENT <= alignof(max_align_t), "DS_ALIGNMENT exceeds the strictest fundamental alignment");
DS_STATIC_ASSERT(DS_MAX_ALLOCATION_SIZE <= DS_BUFFER_MEMORY_SIZE, "DS_MAX_ALLOCATION_SIZE must not exceed DS_BUFFER_MEMORY_SIZE");
DS_STATIC_ASSERT(DS_MAX_ALLOCATION_COUNT > 0u, "DS_MAX_ALLOCATION_COUNT must be positive");
DS_STATIC_ASSERT(DS_MAX_ALLOCATION_SIZE > 0u, "DS_MAX_ALLOCATION_SIZE must be positive");
/* Test premise from Aligned_Size_Exceeds_Remaining_Space, promoted to compile time: */
DS_STATIC_ASSERT((DS_MAX_ALLOCATION_SIZE % DS_ALIGNMENT) == 0u, "DS_MAX_ALLOCATION_SIZE must be a multiple of DS_ALIGNMENT");
/* Overflow guard for ds_align_up (discussed at #7 — closes the implicit
     * size + (DS_ALIGNMENT - 1) wraparound relationship for free): */
DS_STATIC_ASSERT(DS_MAX_ALLOCATION_SIZE <= (SIZE_MAX - DS_ALIGNMENT) + 1u, "ds_align_up may overflow for sizes near SIZE_MAX");

/*---------------Types----------------*/

typedef uint16_t ds_err_code_t; /**< Type of error code used in dynostatic-buffer. */

/**
 * @enum ds_allocator_status_t
 * @brief Current status of allocator.
 */
typedef enum {
    DS_NOT_USED = 0x00, /**< Current allocator is not used to describe memory block. */
    DS_FREE = 0x01,     /**< Current block is used to describe free memory block. */
    DS_ALLOCATED = 0x02 /**< Current block is used to describe allocated memory block. */
} ds_allocator_status_t;

/**
 * @typedef ds_allocator_t
 * @brief Definitions of allocator used to handle information about memory chunks in ds-buffer.
 */
typedef struct {
    size_t head; /**< Place in memory, where allocation part is started. */
    size_t size; /**< Size of current allocated memory chunk. */

    ds_allocator_status_t allocation_status; /**< Current allocator is in use by another parts of code.*/
} ds_allocator_t;

/**
 * @typedef dynostatic_buffer_t
 * @brief Structure describe dynostatic buffer, which is using to emulate dynamic allocation without any allocation in heap.
 */
typedef struct {
    uint8_t memory[DS_BUFFER_MEMORY_SIZE]; /**< Memory declared for buffer. */
    size_t data_head;                      /**< Head of data allocated in buffer. */

    bool initialized; /**< Dynostatic buffer is initialized. */

    ds_allocator_t allocators[DS_MAX_ALLOCATION_COUNT]; /**< List with structure described possible allocations. */
    size_t used_allocators;                             /**< Number of currently used allocators. */
} dynostatic_buffer_t;

/*-----Public-Function-Declaration----*/

/**
 * @brief Initialize allocation buffer
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 *
 * @retval ERROR_DS_OK Dynostatic-buffer is properly initialized.
 * @retval ERROR_DS_INVALID_ARG Some given parameter is incorrect.
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
ds_err_code_t ds_realloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size);

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

#ifdef __cplusplus
}
#endif
