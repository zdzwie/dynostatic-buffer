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

#include <string.h>

#ifdef DS_LOG_ENABLE
    #define DS_LOG(message) dynostatic_buffer.logger(message, strlen(message))
#else
    #define DS_LOG(message) do {} while(0)
#endif

#define DS_ADDR_T unsigned int /**< Define type of address in processor memory. */

/**
 * @enum ds_allocator_status
 * @brief Current status of allocator.
 */
typedef enum {
    DS_NOT_USED  = 0x00, /**< Current allocator is not used to describe memory block. */
    DS_FREE      = 0x01, /**< Current block is used to describe free memory block. */
    DS_ALLOCATED = 0x02  /**< Current block is used to describe allocated memory block. */
} ds_allocator_status;

/**
 * @struct ds_allocator_t
 * @brief Definitions of allocator used to handle information about memory chunks in ds-buffer.
 */
typedef PACK(struct {
    size_t head; /**< Place in memory, where allocation part is started. */
    size_t size; /**< Size of current allocated memory chunk. */

    ds_allocator_status  using; /**< Current allocator is in use by another parts of code.*/
} ds_allocator_t);

/**
 * @struct dynostatic_buffer_t
 * @brief Structure describe dynostatic buffer, which is using to emulate dynamic allocation without any allocation in heap.
 */
static struct {
    uint8_t memory[DS_BUFFER_MEMORY_SIZE]; /**< Memory declared for buffer. */
    size_t data_tail;                      /**< Tail of data allocated in buffer. */

    bool initialized; /**< Dynostatic buffer is initialized. */
    bool full;        /**< There was no empty space in buffer. */
    ds_allocator_t allocators[DS_MAX_ALLOCATION_COUNT];

    #ifdef DS_LOG_ENABLE
    logger_func_t logger; /**< Pointer to function using to print some additional debug information. */
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

    if (dynostatic_buffer.initialized) {
        DS_LOG("Buffer already initialzed!\n\r");
        return EDS_ALREADY_INIT;
    }

    dynostatic_buffer.initialized = true;
    dynostatic_buffer.full = false;

    memset(dynostatic_buffer.memory, 0, DS_BUFFER_MEMORY_SIZE);
    memset(dynostatic_buffer.allocators, 0, sizeof(dynostatic_buffer.allocators));

    DS_LOG("Initialized\n\r");
    return EDS_OK;
}

ds_err_code_t ds_malloc(void **p_memory, size_t size)
{
    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    if ((p_memory == NULL) || (size == 0)) {
        return EDS_INVALID_PARAMS;
    }

    return EDS_OK;
}

ds_err_code_t ds_free(void **p_memory)
{
    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    if ((p_memory == NULL) || (*p_memory == NULL)) {
        return EDS_INVALID_PARAMS;
    }

    if (((uint8_t *)*p_memory < dynostatic_buffer.memory)
        && ((uint8_t *) *p_memory > ((dynostatic_buffer.memory + DS_BUFFER_MEMORY_SIZE)))) {
        DS_LOG("Given pointer is not allocated in DS buffer.\n\r");
        return EDS_MEMORY_OUT_OF_DS;
    }

    *p_memory = NULL;
    return EDS_OK;
}

ds_err_code_t ds_calloc(void **p_memory, size_t len, size_t size_of_elem)
{
    ds_err_code_t ret;

    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    if ((p_memory == NULL) || (*p_memory == NULL) || (len == 0) || (size_of_elem == 0)) {
        return EDS_INVALID_PARAMS;
    }

    ret = ds_malloc(p_memory, (len * size_of_elem));
    DS_VAL_ERR_AND_RET(ret);

    memset(p_memory, 0, (len * size_of_elem));
    return EDS_OK;
}

ds_err_code_t ds_realloc(void **p_memory, size_t size)
{
    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    if ((p_memory == NULL) || (*p_memory == NULL) || (size == 0)) {
        return EDS_INVALID_PARAMS;
    }

    return EDS_OK;
}

ds_err_code_t ds_get_memory_usage(uint8_t *p_memory_usage)
{
    size_t usage = 0;
    if (p_memory_usage == NULL) {
        return EDS_INVALID_PARAMS;
    }

    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    for (uint8_t iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (dynostatic_buffer.allocators[iter].using == DS_ALLOCATED) {
            usage += dynostatic_buffer.allocators[iter].size;
        }
    }

    if (usage > DS_BUFFER_MEMORY_SIZE) {
        *p_memory_usage = 0;
        return EDS_CRITICAL_ERR;
    }

    *p_memory_usage = (uint8_t) ((100 * usage) / DS_BUFFER_MEMORY_SIZE);
    return EDS_OK;
}

ds_err_code_t ds_get_max_new_allocation_size(size_t *p_max_new_allocation)
{
    if (p_max_new_allocation == NULL) {
        return EDS_INVALID_PARAMS;
    }

    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    *p_max_new_allocation = 0;

    for (uint8_t iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (dynostatic_buffer.allocators[iter].using == DS_FREE) {
            if (dynostatic_buffer.allocators[iter].size > *p_max_new_allocation) {
                *p_max_new_allocation = dynostatic_buffer.allocators[iter].size;
            }
        }
    }

    if ((DS_BUFFER_MEMORY_SIZE - dynostatic_buffer.data_tail) > *p_max_new_allocation) {
        *p_max_new_allocation = (DS_BUFFER_MEMORY_SIZE - dynostatic_buffer.data_tail);
    }

    if (*p_max_new_allocation > DS_MAX_ALLOCATION_SIZE ) {
        *p_max_new_allocation = DS_MAX_ALLOCATION_SIZE;
    }

    return EDS_OK;
}

ds_err_code_t ds_get_free_allocator_cnt(size_t *p_free_allocators)
{
    if (p_free_allocators == NULL) {
        return EDS_INVALID_PARAMS;
    }

    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    *p_free_allocators = 0;
    for (uint8_t iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (dynostatic_buffer.allocators[iter].using == DS_NOT_USED) {
            *p_free_allocators++;
        }
    }

    return EDS_OK;
}

#ifdef TEST_LIBRARY
void ds_deinit_allocation(void)
{
    dynostatic_buffer.initialized = false;
}
#endif
