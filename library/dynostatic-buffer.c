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
 * @brief Zero a memory region with an explicit destination-size guard.
 *
 * Mirrors the memset_s(dest, dest_size, 0, count) shape: the caller states
 * how much writable space the destination has, and the guard rejects any
 * attempt to zero beyond it. Centralizes the single sanctioned use of
 * memset so analyzer suppressions live in exactly one place (ds_memcpy is
 * the copying counterpart).
 *
 * @pre p_dest != NULL and size_to_zero <= dest_size — enforced by DS_ASSERT
 *      only: caught in debug builds, undefined behaviour once NDEBUG strips
 *      the asserts.
 *
 * @param[out] p_dest Memory to zero.
 * @param[in] dest_size Guaranteed writable size of the destination — the
 *                      lower bound known to the caller, not necessarily the
 *                      region's full capacity.
 * @param[in] size_to_zero Number of bytes to set to zero.
 */
static inline void ds_zero(void *p_dest, size_t dest_size, size_t size_to_zero);

/**
 * @brief Perform bounds-guarded memory copy between non-overlapping regions.
 *
 * Centralizes the single sanctioned use of memcpy so that any analyzer
 * suppressions (clang-tidy insecureAPI today, future MISRA deviations)
 * live in exactly one place.
 *
 * @pre Regions must not overlap (memcpy semantics); callers guarantee this
 *      structurally — distinct allocator blocks never alias.
 *
 * @param[out] p_dest Destination memory.
 * @param[in] dest_size Guaranteed writable size of the destination — the
 *                      lower bound known to the caller, not necessarily the
 *                      block's full capacity.
 * @param[in] p_src Source memory.
 * @param[in] size_to_copy Number of bytes to copy.
 */
static inline void ds_memcpy(void *p_dest, size_t dest_size, const void *p_src, size_t size_to_copy);

/**
 * @brief Assign an allocator record and a memory region for a new block.
 *
 * Serves the request reuse-first: scans touched records for a parked
 * DS_FREE block whose capacity fits the ALIGNED size, and only then falls
 * back to a fresh bump allocation at data_head. In both paths the record
 * is marked DS_ALLOCATED, used_allocators is incremented, and *p_alloc_idx
 * receives the record's index. A reused record KEEPS its original capacity;
 * a fresh record gets capacity ds_align_up(size). The scan stops at the
 * first DS_NOT_USED record (compact-prefix invariant, see ds_allocator_t).
 *
 * @pre p_ds_buffer and p_alloc_idx are non-NULL; the instance is
 *      initialized; size passed the public ds_malloc validation — the
 *      internal size re-check is defensive only and unreachable through
 *      the public API.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in] size Requested size in bytes (raw; alignment is applied here).
 * @param[out] p_alloc_idx Index of the assigned record; written only on
 *                         ERROR_DS_OK.
 *
 * @retval ERROR_DS_OK Record assigned; *p_alloc_idx identifies it.
 * @retval ERROR_DS_INVALID_ARG Defensive size re-check failed (dead branch
 *                              through the public API).
 * @retval ERROR_DS_NO_ALLOCATORS Either DS_MAX_ALLOCATION_COUNT blocks are
 *                                already live, or every record is occupied
 *                                (live, or parked with too small a
 *                                capacity) and no DS_NOT_USED slot remains
 *                                for a bump allocation.
 * @retval ERROR_DS_NO_MEMORY A record is available but the bump space
 *                            cannot hold the aligned size.
 */
static ds_err_code_t ds_get_new_allocator(dynostatic_buffer_t *p_ds_buffer, size_t size, size_t *p_alloc_idx);

/**
 * @brief Find the allocator record whose live block starts at @p p_memory.
 *
 * Reduces the pointer to an arena offset and matches it against the head
 * of DS_ALLOCATED records. Only exact block starts match: interior
 * pointers, parked DS_FREE blocks and reclaimed addresses do not. The scan
 * stops at the first DS_NOT_USED record (compact-prefix invariant, see
 * ds_allocator_t). The two failure codes are deliberately diagnostic:
 * outside the arena (foreign pointer, likely an application logic bug)
 * versus inside but not a live block start (double free or pointer
 * arithmetic gone wrong).
 *
 * This function owns the project's MISRA 11.4/11.6 deviations: integer
 * comparison of addresses is the defined-behaviour alternative to a
 * relational comparison of unrelated pointers.
 *
 * @pre p_ds_buffer and p_alloc_idx are non-NULL; the instance is
 *      initialized. p_memory may hold ANY value — NULL, garbage, a stale
 *      alias — all safely fail the range or match checks (relied upon by
 *      the ds_malloc PTR_ALLOC_YET pre-check).
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in] p_memory Address to look up.
 * @param[out] p_alloc_idx Index of the matching record; written only on
 *                         ERROR_DS_OK.
 *
 * @retval ERROR_DS_OK Live block found; *p_alloc_idx identifies its record.
 * @retval ERROR_DS_MEMORY_OUT_OF_DS p_memory lies outside this instance's
 *                                   arena.
 * @retval ERROR_DS_ALLOCATOR_NOT_FOUND p_memory is inside the arena but is
 *                                      not the start of a live block.
 */
static ds_err_code_t ds_find_allocator_for_memory(const dynostatic_buffer_t *p_ds_buffer, void *p_memory, size_t *p_alloc_idx);

/**
 * @brief Round @p size up to the nearest multiple of DS_ALIGNMENT.
 *
 * Power-of-two mask arithmetic — (size + mask) & ~mask — with every operand
 * in size_t from the first operation (MISRA 10.7). Returns size unchanged
 * when it is already a multiple of DS_ALIGNMENT.
 *
 * @pre size <= DS_MAX_ALLOCATION_SIZE: together with the header's
 *      DS_STATIC_ASSERT (DS_MAX_ALLOCATION_SIZE <= SIZE_MAX - DS_ALIGNMENT
 *      + 1) this makes the internal addition overflow-free.
 *
 * @param[in] size Size in bytes to align.
 *
 * @return The smallest multiple of DS_ALIGNMENT that is >= size.
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
 * @note Does NOT modify used_allocators: cascaded DS_FREE records were
 *       already discounted at their own ds_free; the caller decrements
 *       exactly once, for the block currently being freed.
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

static inline void ds_memcpy(void *p_dest, size_t dest_size, const void *p_src, size_t size_to_copy)
{
    DS_ASSERT(p_dest != NULL);
    DS_ASSERT(p_src != NULL);
    DS_ASSERT(size_to_copy <= dest_size); /* the destination-size guard memcpy_s adds */

    (void)memcpy(p_dest, p_src, size_to_copy); /* NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */

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

    const size_t head = p_ds_buffer->allocators[alloc_idx].head;
    const size_t size = p_ds_buffer->allocators[alloc_idx].size;

#if DS_ZERO_ON_FREE == 0u
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

    ds_zero(*p_memory, ds_align_up(total_size), total_size);
    return ERROR_DS_OK;
}

ds_err_code_t ds_realloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size)
{
    if ((NULL == p_ds_buffer) || (NULL == p_memory)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (DS_MAGIC_NUMBER != p_ds_buffer->init_magic) {
        return ERROR_DS_NO_INIT;
    }

    if (size == 0u) {
        return ds_free(p_ds_buffer, p_memory);
    }

    if (*p_memory == NULL) {
        return ds_malloc(p_ds_buffer, p_memory, size);
    }

    if (size > DS_MAX_ALLOCATION_SIZE) {
        return ERROR_DS_TOO_BIG_CHUNK;
    }

    size_t alloc_idx;
    ds_err_code_t ret = ds_find_allocator_for_memory(p_ds_buffer, *p_memory, &alloc_idx);
    if (ret != ERROR_DS_OK) {
        return ret; /* OUT_OF_DS / ALLOCATOR_NOT_FOUND — original contract bug fixed */
    }

    const size_t head = p_ds_buffer->allocators[alloc_idx].head;
    const size_t capacity = p_ds_buffer->allocators[alloc_idx].size;
    const size_t aligned_size = ds_align_up(size);

    if (aligned_size <= capacity) {
        return ERROR_DS_OK; /* shrink or fit: in place, capacity retained (no split) */
    }

    /* Trailing-block fast path: grow in place by advancing the bump head. */
    if (((head + capacity) == p_ds_buffer->data_head)
        && ((DS_BUFFER_MEMORY_SIZE - head) >= aligned_size)) {
        p_ds_buffer->data_head = head + aligned_size;
        p_ds_buffer->allocators[alloc_idx].size = aligned_size;
        return ERROR_DS_OK;
    }

    /* Move path: allocate FIRST, so any failure leaves the original intact. */
    void *p_new = NULL;
    ret = ds_malloc(p_ds_buffer, &p_new, size);
    if (ret != ERROR_DS_OK) {
        return ret;
    }

    ds_memcpy(p_new, aligned_size, *p_memory, capacity);

    ret = ds_free(p_ds_buffer, p_memory); /* zeroes old block under DS_ZERO_ON_FREE; may cascade */
    DS_ASSERT(ret == ERROR_DS_OK);
    (void)ret;

    *p_memory = p_new;
    return ERROR_DS_OK;
}

ds_err_code_t ds_get_memory_usage(const dynostatic_buffer_t *p_ds_buffer, uint8_t *p_memory_usage)
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

ds_err_code_t ds_get_max_new_allocation_size(const dynostatic_buffer_t *p_ds_buffer, size_t *p_max_new_allocation)
{
    if ((NULL == p_ds_buffer) || (NULL == p_max_new_allocation)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (DS_MAGIC_NUMBER != p_ds_buffer->init_magic) {
        return ERROR_DS_NO_INIT;
    }

    size_t max_size = 0u;

    if (p_ds_buffer->used_allocators < DS_MAX_ALLOCATION_COUNT) {
        bool bump_slot_available = false;

        /* Reuse candidate: the largest parked DS_FREE capacity.
         * Bump candidate exists only if a DS_NOT_USED slot does. */
        for (size_t iter = 0u; iter < DS_MAX_ALLOCATION_COUNT; iter++) {
            const ds_allocator_status_t status = p_ds_buffer->allocators[iter].allocation_status;
            if (status == DS_NOT_USED) {
                bump_slot_available = true;
                break; /* compact prefix: nothing lives past this point */
            }
            if ((status == DS_FREE) && (p_ds_buffer->allocators[iter].size > max_size)) {
                max_size = p_ds_buffer->allocators[iter].size;
            }
        }

        if (bump_slot_available) {
            const size_t remaining = DS_BUFFER_MEMORY_SIZE - p_ds_buffer->data_head;
            /* data_head is alignment-multiple, so only an unaligned buffer
             * tail can make `remaining` unaligned; that tail is unusable. */
            size_t bump_max = remaining - (remaining % DS_ALIGNMENT);
            if (bump_max > DS_MAX_ALLOCATION_SIZE) {
                bump_max = DS_MAX_ALLOCATION_SIZE;
            }
            if (bump_max > max_size) {
                max_size = bump_max;
            }
        }
    }

    *p_max_new_allocation = max_size;
    return ERROR_DS_OK;
}

ds_err_code_t ds_get_free_allocator_cnt(const dynostatic_buffer_t *p_ds_buffer, size_t *p_free_allocators)
{
    if ((NULL == p_ds_buffer) || (NULL == p_free_allocators)) {
        return ERROR_DS_INVALID_ARG;
    }

    if (DS_MAGIC_NUMBER != p_ds_buffer->init_magic) {
        return ERROR_DS_NO_INIT;
    }

    *p_free_allocators = DS_MAX_ALLOCATION_COUNT - p_ds_buffer->used_allocators;
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
