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
#include <stdint.h>
#include <assert.h>

/*---------Macros-And-Defines---------*/

#ifndef DS_ASSERT
    /**
     * @brief Wrapper for assertion
     * @param[in] cond Condition to check.
     */
    #define DS_ASSERT(cond) assert(cond)
#endif

#define DS_MAGIC_NUMBER (0x1525u) /**< Magic number to check that DS-buffer is initialized or not. */

/*-----Static-Function-Prototypes-----*/

/**
 * @brief Perform secure memset with zeros of memory.
 *
 * @param[in, out] p_dest Pointer to memory, which will be filled with given value.
 * @param[in] dest_size Size of memory, which should be set.
 * @param[in] size_to_zero Size of memory, which should be filled with zeros.
 */
static inline void ds_zero(void *p_dest, size_t dest_size, size_t size_to_zero);

/**
 * @brief Get new allocator for dynostatic-buffer.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in] size Size of memory chunk which will be allocated.
 * @param[out] p_alloc_idx Pointer to variable, where index of new allocator will be saved.
 *
 * @retval ERROR_DS_OK Memory is properly allocated in dynostatic-buffer.
 * @retval ERROR_DS_NO_MEMORY There is not enough free memory to allocate demanded memory chunk.
 * @retval ERROR_DS_NO_ALLOCATORS There is not enough free allocators to use.
 * @retval ERROR_DS_INVALID_ARG Given parameters are invalid.
 */
static ds_err_code_t ds_get_new_allocator(dynostatic_buffer_t *p_ds_buffer, size_t size, size_t *p_alloc_idx);

/**
 * @brief Get allocator matched to given pointer.
 *
 * @param p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param p_memory Pointer to memory, which allocator should be found.
 * @param p_alloc_idx Pointer to variable, where index of found allocator will be saved.
 *
 * @retval ERROR_DS_OK Allocator is properly found.
 *
 */
static ds_err_code_t ds_find_allocator_for_memory(const dynostatic_buffer_t *p_ds_buffer, void *p_memory, size_t *p_alloc_idx);

/**
 * @brief Align size up to the nearest multiple of alignment.
 *
 * @param[in] size Size to be aligned.
 *
 * @return Aligned size.
 */
static inline size_t ds_align_up(size_t size);

/**
 * @brief Reclaim the trailing block and cascade through any DS_FREE blocks
 *        directly beneath it, rolling data_head back past all of them.
 *
 * Correctness rests on two invariants of bump allocation: (a) among touched
 * slots, index order equals head order, so the trailing block always lives
 * at the highest touched index; (b) touched slots form a gapless partition
 * of [0, data_head), so after reclaiming slot idx the block ending exactly
 * at the new data_head is always slot idx - 1.
 *
 * @pre allocators[alloc_idx] is the trailing block (head + size == data_head).
 * @pre Block contents were already zeroed by their respective ds_free calls
 *      (when DS_ZERO_ON_FREE is enabled) — this function only clears records.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in] alloc_idx Index of the trailing block's allocator.
 */
static void ds_reclaim_trailing(dynostatic_buffer_t *p_ds_buffer, size_t alloc_idx);

/*---Static-Function-Implementation---*/

static inline void ds_zero(void *p_dest, size_t dest_size, size_t size_to_zero)
{
    DS_ASSERT(p_dest != NULL);
    DS_ASSERT(size_to_zero <= dest_size); /* the destination-size_to_zero guard memset_s adds */

    (void)memset(p_dest, 0u, size_to_zero); /* NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */

    (void)dest_size;
}

static ds_err_code_t ds_get_new_allocator(dynostatic_buffer_t *p_ds_buffer, size_t size, size_t *p_alloc_idx)
{
    size_t iter;
    if ((size == 0u) || (size > DS_MAX_ALLOCATION_SIZE)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->used_allocators >= DS_MAX_ALLOCATION_COUNT) {
        return ERROR_DS_NO_ALLOCATORS;
    }

    const size_t aligned_size = ds_align_up(size);

    for (iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (p_ds_buffer->allocators[iter].allocation_status == DS_FREE) {
            if (p_ds_buffer->allocators[iter].size >= aligned_size) {
                p_ds_buffer->allocators[iter].allocation_status = DS_ALLOCATED;
                p_ds_buffer->used_allocators++;
                *p_alloc_idx = iter;
                return ERROR_DS_OK;
            }
        } else if (p_ds_buffer->allocators[iter].allocation_status == DS_NOT_USED) {
            break;
        } else {
            /* No action */
        }
    }

    if (iter == DS_MAX_ALLOCATION_COUNT) {
        return ERROR_DS_NO_ALLOCATORS;
    }

    if ((DS_BUFFER_MEMORY_SIZE - p_ds_buffer->data_head) < aligned_size) {
        return ERROR_DS_NO_MEMORY;
    }

    p_ds_buffer->allocators[iter].allocation_status = DS_ALLOCATED;
    p_ds_buffer->allocators[iter].size = aligned_size;
    p_ds_buffer->allocators[iter].head = p_ds_buffer->data_head;
    p_ds_buffer->data_head += aligned_size;
    p_ds_buffer->used_allocators++;

    *p_alloc_idx = iter;

    return ERROR_DS_OK;
}

static inline size_t ds_align_up(size_t size)
{
    const size_t mask = (size_t)DS_ALIGNMENT - 1u;
    return (size + mask) & ~mask;
}

static ds_err_code_t ds_find_allocator_for_memory(const dynostatic_buffer_t *p_ds_buffer, void *p_memory, size_t *p_alloc_idx)
{
    /* cppcheck-suppress misra-c2012-11.6 ; deviation: ... */
    const uintptr_t addr = (uintptr_t)p_memory;
    /* cppcheck-suppress misra-c2012-11.4 ; deviation: ... */
    const uintptr_t start = (uintptr_t)p_ds_buffer->memory;

    if ((addr < start) || ((addr - start) >= (uintptr_t)DS_BUFFER_MEMORY_SIZE)) {
        return ERROR_DS_MEMORY_OUT_OF_DS;
    }

    const size_t offset = (size_t)(addr - start);

    for (size_t iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (DS_NOT_USED == p_ds_buffer->allocators[iter].allocation_status) {
            break; /* Nothing lives past the first NOT_USED */
        }

        if ((DS_ALLOCATED == p_ds_buffer->allocators[iter].allocation_status) && (offset == p_ds_buffer->allocators[iter].head)) {
            *p_alloc_idx = iter;
            return ERROR_DS_OK;
        }
    }

    return ERROR_DS_ALLOCATOR_NOT_FOUND;
}

static void ds_reclaim_trailing(dynostatic_buffer_t *p_ds_buffer, size_t alloc_idx)
{
    size_t idx = alloc_idx;
    bool cascade = true;

    while (cascade) {
        p_ds_buffer->data_head = p_ds_buffer->allocators[idx].head;
        p_ds_buffer->allocators[idx].allocation_status = DS_NOT_USED;
        p_ds_buffer->allocators[idx].head = 0u;
        p_ds_buffer->allocators[idx].size = 0u;

        if ((0u == idx) || (DS_FREE != p_ds_buffer->allocators[idx - 1u].allocation_status)) {
            cascade = false;
        } else {
            idx--;
        }
    }
}

/*---Public-Function-Implementation---*/

ds_err_code_t ds_initialize_allocation(dynostatic_buffer_t *p_ds_buffer)
{
    if (NULL == p_ds_buffer) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->init_magic == DS_MAGIC_NUMBER) {
        return ERROR_DS_ALREADY_INIT;
    }

    p_ds_buffer->init_magic = DS_MAGIC_NUMBER;
    p_ds_buffer->data_head = 0;
    p_ds_buffer->used_allocators = 0;

    ds_zero(p_ds_buffer->memory, DS_BUFFER_MEMORY_SIZE, DS_BUFFER_MEMORY_SIZE);
    ds_zero(p_ds_buffer->allocators, sizeof(p_ds_buffer->allocators), sizeof(p_ds_buffer->allocators));

    return ERROR_DS_OK;
}

ds_err_code_t ds_malloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size)
{
    if ((NULL == p_ds_buffer) || (NULL == p_memory) || (size == 0u)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->init_magic != DS_MAGIC_NUMBER) {
        return ERROR_DS_NO_INIT;
    }

    if (size > DS_MAX_ALLOCATION_SIZE) {
        return ERROR_DS_TOO_BIG_CHUNK;
    }

    size_t alloc_idx;
    ds_err_code_t ret = ds_find_allocator_for_memory(p_ds_buffer, *p_memory, &alloc_idx);
    if (ret == ERROR_DS_OK) {
        return ERROR_DS_PTR_ALLOC_YET;
    }

    ret = ds_get_new_allocator(p_ds_buffer, size, &alloc_idx);
    if (ret != ERROR_DS_OK) {
        return ret;
    }

    *p_memory = &p_ds_buffer->memory[p_ds_buffer->allocators[alloc_idx].head];
    return ERROR_DS_OK;
}

ds_err_code_t ds_free(dynostatic_buffer_t *p_ds_buffer, void **p_memory)
{
    if ((NULL == p_ds_buffer) || (NULL == p_memory) || (NULL == *p_memory)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->init_magic != DS_MAGIC_NUMBER) {
        return ERROR_DS_NO_INIT;
    }

    size_t alloc_idx;
    ds_err_code_t ret = ds_find_allocator_for_memory(p_ds_buffer, *p_memory, &alloc_idx);
    if (ret != ERROR_DS_OK) {
        return ret;
    }

#ifdef DS_ZERO_ON_FREE
    const size_t head = p_ds_buffer->allocators[alloc_idx].head;
    const size_t size = p_ds_buffer->allocators[alloc_idx].size;
    ds_zero(&p_ds_buffer->memory[head], DS_BUFFER_MEMORY_SIZE - head, size);
#endif

    if ((head + size) == p_ds_buffer->data_head) {
        ds_reclaim_trailing(p_ds_buffer, alloc_idx);
    } else {
        p_ds_buffer->allocators[alloc_idx].allocation_status = DS_FREE;
    }

    p_ds_buffer->used_allocators--;
    *p_memory = NULL;
    return ERROR_DS_OK;
}

ds_err_code_t ds_calloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t len, size_t size_of_elem)
{
    if ((NULL == p_ds_buffer) || (NULL == p_memory) || (len == 0u) || (size_of_elem == 0u)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->init_magic != DS_MAGIC_NUMBER) {
        return ERROR_DS_NO_INIT;
    }

    size_t total_size = len * size_of_elem;
    if ((total_size / len) != size_of_elem) { /* Check for overflow */
        return ERROR_DS_INVALID_ARG;
    }

    ds_err_code_t ret = ds_malloc(p_ds_buffer, p_memory, total_size);
    if (ret != ERROR_DS_OK) {
        return ret;
    }

    ds_zero(*p_memory, DS_BUFFER_MEMORY_SIZE, total_size);
    return ERROR_DS_OK;
}

ds_err_code_t ds_realloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size)
{
    if ((NULL == p_ds_buffer) || (p_memory == NULL)) {
        return ERROR_DS_INVALID_ARG;
    }

    if ((p_ds_buffer->init_magic != DS_MAGIC_NUMBER)) {
        return ERROR_DS_NO_INIT;
    }

    if (size == 0u) {
        return ds_free(p_ds_buffer, p_memory);
    }

    if (*p_memory == NULL) {
        return ds_malloc(p_ds_buffer, p_memory, size);
    }

    // TODO: Finish implementation
    return ERROR_DS_CRITICAL_ERR;
}

ds_err_code_t ds_get_memory_usage(dynostatic_buffer_t *p_ds_buffer, uint8_t *p_memory_usage)
{
    size_t usage = 0;
    if ((NULL == p_ds_buffer) || (p_memory_usage == NULL)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->init_magic != DS_MAGIC_NUMBER) {
        return ERROR_DS_NO_INIT;
    }

    for (size_t iter = 0; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
        if (p_ds_buffer->allocators[iter].allocation_status == DS_ALLOCATED) {
            usage += p_ds_buffer->allocators[iter].size;
        }
    }

    if (usage > DS_BUFFER_MEMORY_SIZE) {
        *p_memory_usage = 0;
        return ERROR_DS_CRITICAL_ERR;
    }

    *p_memory_usage = (uint8_t)((100u * usage) / DS_BUFFER_MEMORY_SIZE);
    return ERROR_DS_OK;
}

ds_err_code_t ds_deinit_allocation(dynostatic_buffer_t *p_ds_buffer)
{
    if (NULL == p_ds_buffer) {
        return ERROR_DS_INVALID_ARG;
    }

    if (p_ds_buffer->init_magic != DS_MAGIC_NUMBER) {
        return ERROR_DS_NO_INIT;
    }

    p_ds_buffer->init_magic = 0;
    ds_zero(p_ds_buffer->memory, DS_BUFFER_MEMORY_SIZE, DS_BUFFER_MEMORY_SIZE);
    ds_zero(p_ds_buffer->allocators, sizeof(p_ds_buffer->allocators), sizeof(p_ds_buffer->allocators));
    p_ds_buffer->data_head = 0;
    p_ds_buffer->used_allocators = 0;
    return ERROR_DS_OK;
}
