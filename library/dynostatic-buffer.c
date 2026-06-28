/**
 * @file    dynostatic-buffer.c
 * @brief   Implementation of dynamic allocation of memory in static buffer.
 *
 * @author  Jakub Brzezowski
 * @date    2022-06-14
 */

 #include "dynostatic-buffer.h"

 #include <string.h>
 #include <stdbool.h>

/* *INDENT-OFF* */
 #ifdef __cplusplus
 extern "C" {
 #endif
/* *INDENT-ON* */

 #define DS_GET_USED_ALLOCATOR(ALLOCATOR, SIZE) ALLOCATOR.allocation_status = DS_ALLOCATED; ALLOCATOR.size = SIZE /**< Macro to inserting size for given allocator. */

/**
 * @brief Get new allocator for dynostatic-buffer.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in] size Size of memory chunk which will be allocated.
 *
 * @retval ERROR_DS_OK Memory is properly allocated in dynostatic-buffer.
 * @retval ERROR_DS_NO_MEMORY There is not enough free memory to allocate demanded memory chunk.
 * @retval ERROR_DS_NO_ALLOCATORS There is not enough free allocators to use.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 */
static ds_err_code_t ds_get_new_allocator(dynostatic_buffer_t *p_ds_buffer, size_t size);

static ds_err_code_t ds_get_new_allocator(dynostatic_buffer_t *p_ds_buffer, size_t size)
{
    size_t iter;
    if ((size == 0) || size > DS_MAX_ALLOCATION_SIZE) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->used_allocators >= DS_MAX_ALLOCATION_COUNT) {
        return ERROR_DS_NO_ALLOCATORS;
    }

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (p_ds_buffer->allocators[iter].allocation_status == DS_FREE) {
            if (p_ds_buffer->allocators[iter].size >= size) {
                DS_GET_USED_ALLOCATOR(p_ds_buffer->allocators[iter], size);
                return ERROR_DS_OK;
            }
        } else if (p_ds_buffer->allocators[iter].allocation_status == DS_NOT_USED) {
            break;
        }
    }

    if ((DS_BUFFER_MEMORY_SIZE - p_ds_buffer->data_head) < size) {
        return ERROR_DS_NO_MEMORY;
    }

    if (iter == DS_MAX_ALLOCATION_COUNT) {
        return ERROR_DS_NO_ALLOCATORS;
    }

    p_ds_buffer->allocators[iter].allocation_status = DS_ALLOCATED;
    p_ds_buffer->allocators[iter].size = size;
    p_ds_buffer->allocators[iter].head = p_ds_buffer->data_head;
    p_ds_buffer->data_head += size;

    return ERROR_DS_OK;
}

ds_err_code_t ds_initialize_allocation(dynostatic_buffer_t *p_ds_buffer)
{
    if (p_ds_buffer == NULL) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->initialized) {
        return ERROR_DS_ALREADY_INIT;
    }

    p_ds_buffer->initialized = true;

    memset(p_ds_buffer->memory, 0, DS_BUFFER_MEMORY_SIZE);
    memset(p_ds_buffer->allocators, 0, sizeof(p_ds_buffer->allocators));

    return ERROR_DS_OK;
}

ds_err_code_t ds_malloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size)
{
    if (!p_ds_buffer->initialized) {
        return ERROR_DS_NO_INIT;
    }

    if ((p_memory == NULL) || (size == 0)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (size > DS_MAX_ALLOCATION_SIZE) {
        return ERROR_DS_TOO_BIG_CHUNK;
    }

    ds_err_code_t ret;
    ret = ds_get_new_allocator(p_ds_buffer, size);
    if (ret != ERROR_DS_OK) {
        return ret;
    }

    *p_memory = (p_ds_buffer->memory + p_ds_buffer->data_head);
    return ERROR_DS_OK;
}

ds_err_code_t ds_free(dynostatic_buffer_t *p_ds_buffer, void **p_memory)
{
    if (!p_ds_buffer->initialized) {
        return ERROR_DS_NO_INIT;
    }

    if ((p_memory == NULL) || (*p_memory == NULL)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (((uint8_t *)*p_memory < p_ds_buffer->memory)
        && ((uint8_t *) *p_memory > ((p_ds_buffer->memory + DS_BUFFER_MEMORY_SIZE)))) {
        return ERROR_DS_MEMORY_OUT_OF_DS;
    }

    /* TODO: End implementation. */

    p_ds_buffer->used_allocators--;

    *p_memory = NULL;
    return ERROR_DS_OK;
}

ds_err_code_t ds_calloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t len, size_t size_of_elem)
{
    if (!p_ds_buffer->initialized) {
        return ERROR_DS_NO_INIT;
    }

    if ((p_memory == NULL) || (len == 0) || (size_of_elem == 0)) {
        return ERROR_DS_INVALID_ARG;
    }

    size_t total_size = len * size_of_elem;
    if (total_size / len != size_of_elem) { /* Check for overflow */
        return ERROR_DS_INVALID_ARG;
    }

    ds_err_code_t ret = ds_malloc(p_ds_buffer, p_memory, total_size);
    if (ret != ERROR_DS_OK) {
        return ret;
    }

    memset(*p_memory, 0, total_size);
    return ERROR_DS_OK;
}

ds_err_code_t ds_realloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size)
{
    if (!p_ds_buffer->initialized) {
        return ERROR_DS_NO_INIT;
    }

    if (p_memory == NULL) {
        return ERROR_DS_INVALID_ARG;
    }

    if (size == 0) {
        return ds_free(p_ds_buffer, p_memory);
    }

    if (*p_memory == NULL) {
        return ds_malloc(p_ds_buffer, p_memory, size);
    }

    // TODO: Finish implementation
    return ERROR_DS_OK;
}

ds_err_code_t ds_get_memory_usage(dynostatic_buffer_t *p_ds_buffer, uint8_t *p_memory_usage)
{
    size_t usage = 0;
    if (p_memory_usage == NULL) {
        return ERROR_DS_INVALID_ARG;
    }

    if (!p_ds_buffer->initialized) {
        return ERROR_DS_NO_INIT;
    }

    for (uint8_t iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (p_ds_buffer->allocators[iter].allocation_status == DS_ALLOCATED) {
            usage += p_ds_buffer->allocators[iter].size;
        }
    }

    if (usage > DS_BUFFER_MEMORY_SIZE) {
        *p_memory_usage = 0;
        return ERROR_DS_CRITICAL_ERR;
    }

    *p_memory_usage = (uint8_t) ((100 * usage) / DS_BUFFER_MEMORY_SIZE);
    return ERROR_DS_OK;
}

ds_err_code_t ds_deinit_allocation(dynostatic_buffer_t *p_ds_buffer)
{
    if (!p_ds_buffer->initialized) {
        return ERROR_DS_NO_INIT;
    }

    p_ds_buffer->initialized = false;
    bzero(p_ds_buffer->memory, DS_BUFFER_MEMORY_SIZE);
    bzero(p_ds_buffer->allocators, sizeof(p_ds_buffer->allocators));
    p_ds_buffer->data_head = 0;
    p_ds_buffer->used_allocators = 0;
    return ERROR_DS_OK;
}

/* *INDENT-OFF* */
 #ifdef __cplusplus
 }
 #endif
/* *INDENT-ON* */
