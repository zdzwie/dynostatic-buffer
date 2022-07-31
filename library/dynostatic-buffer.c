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
#include <stdio.h>

/*<<<<----  Macros and defines ---->>>>*/

#ifdef DS_LOG_ENABLE
    #define DS_LOG(message) dynostatic_buffer.logger(message, strlen(message))
#else
    #define DS_LOG(message) do {} while(0)
#endif

/**
 * @def DS_GET_USED_ALLOCATOR
 * @brief Get allocator, which was used in recent freed allocation.
 *
 * @param[in,out] ALLOCATOR Used allocator, which can be used again.
 * @param[in] SIZE Size of new allocation.
 */
#define DS_GET_USED_ALLOCATOR(ALLOCATOR, SIZE) \
    ALLOCATOR.using = DS_ALLOCATED;            \
    ALLOCATOR.size = SIZE                      \
/*<<<<----  Typedefs ---->>>>*/

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
    size_t data_head;                      /**< Head of data allocated in buffer. */

    bool initialized; /**< Dynostatic buffer is initialized. */

    ds_allocator_t allocators[DS_MAX_ALLOCATION_COUNT]; /**< List with structure described possible allocations. */
    size_t used_allocators;                             /**< Number of currently used allocators. */

    #ifdef DS_LOG_ENABLE
    logger_func_t logger; /**< Pointer to function using to print some additional debug information. */
    #endif
} dynostatic_buffer;

/*<<<<----  Static functions definitions ---->>>>*/

/**
 * @brief Get first free allocator, which can handle demanded memory size.
 *
 * @param[in]  size Size of demanded allocation size.
 *
 * @retval EDS_OK  New allocator is properly get.
 * @retval EDS_INVALID_PARAMS Some of given parameter is invalid.
 * @retval EDS_NO_ALLOCATORS There is no free allocators to assign.
 * @retval EDS_NO_MEMORY Not enough free memory for new allocation.
 */
static ds_err_code_t ds_get_new_allocator(size_t size);

/*<<<<----  Static functions implementations ---->>>>*/

static ds_err_code_t ds_get_new_allocator(size_t size)
{
    size_t free_allocators = 0;
    size_t iter;
    if ((p_allocator == NULL) || (size == 0) || size > DS_MAX_ALLOCATION_SIZE) {
        return EDS_INVALID_PARAMS;
    }

    if (dynostatic_buffer.used_allocators == DS_MAX_ALLOCATION_COUNT) {
        return EDS_NO_ALLOCATORS;
    }

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (dynostatic_buffer.allocators[iter].using == DS_FREE) {
            if (dynostatic_buffer.allocators[iter].size >= size) {
                DS_GET_USED_ALLOCATOR(dynostatic_buffer.allocators[iter],size);
                return EDS_OK;
            }
        } else if (dynostatic_buffer.allocators[iter].using == DS_NOT_USED) {
            break;
        }
    }

    if ((DS_BUFFER_MEMORY_SIZE - dynostatic_buffer.data_head) < size) {
        return EDS_NO_MEMORY;
    }

    dynostatic_buffer.allocators[iter].using = DS_ALLOCATED;
    dynostatic_buffer.allocators[iter].size = size;
    dynostatic_buffer.allocators[iter].head = dynostatic_buffer.data_head;
    dynostatic_buffer.data_head += size;


    return EDS_OK;
}

/*<<<<----  Public functions ---->>>>*/

ds_err_code_t ds_initialize_allocation(logger_func_t logger)
{
    #ifdef DS_LOG_ENABLE
    if (logger == NULL) {
        return EDS_INVALID_PARAMS;
    }
    #endif

    dynostatic_buffer.logger = logger;

    if (dynostatic_buffer.initialized) {
        DS_LOG("Buffer already initialized!\n\r");
        return EDS_ALREADY_INIT;
    }

    dynostatic_buffer.initialized = true;

    memset(dynostatic_buffer.memory, 0, DS_BUFFER_MEMORY_SIZE);
    memset(dynostatic_buffer.allocators, 0, sizeof(dynostatic_buffer.allocators));

    DS_LOG("Initialized\n\r");
    return EDS_OK;
}

ds_err_code_t ds_malloc(void **p_memory, size_t size)
{
    ds_err_code_t ret;
    if (!dynostatic_buffer.initialized) {
        return EDS_NO_INIT;
    }

    if ((p_memory == NULL) || (size == 0)) {
        return EDS_INVALID_PARAMS;
    }

    if (size > DS_MAX_ALLOCATION_SIZE) {
        return EDS_TOO_BIG_CHUNK;
    }

    ret = ds_get_new_allocator(size);
    if (ret != EDS_OK) {
        return  ret;
    }

    *p_memory = (dynostatic_buffer.memory + dynostatic_buffer.data_head);
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

    if ((DS_BUFFER_MEMORY_SIZE - dynostatic_buffer.data_head) > *p_max_new_allocation) {
        *p_max_new_allocation = (DS_BUFFER_MEMORY_SIZE - dynostatic_buffer.data_head);
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

    if (*p_free_allocators >= DS_MAX_ALLOCATION_COUNT) {
        return EDS_CRITICAL_ERR;
    }

    return EDS_OK;
}

#ifdef TEST_LIBRARY
void ds_deinit_allocation(void)
{
    dynostatic_buffer.initialized = false;
    memset(dynostatic_buffer.allocators, 0, sizeof (dynostatic_buffer.allocators));
    dynostatic_buffer.used_allocators = 0;
}
#endif
