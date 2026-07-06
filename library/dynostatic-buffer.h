/**
 * @file    dynostatic-buffer.h
 * @brief   Implementation of dynamic allocation of memory in static buffer.
 *
 * @author  Jakub Brzezowski
 * @date    2022-06-14
 */

#ifndef DYNOSTATIC_BUFFER_H
#define DYNOSTATIC_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------Macros-And-Defines---------*/

/**
 * @defgroup ERR_CODES_DS Definitions of dynostatic-buffer error codes.
 * @{
 */
#define ERROR_DS_OK                  ((ds_err_code_t)(0x00u)) /**< All goes ok. Any error reported.*/
#define ERROR_DS_NO_INIT             ((ds_err_code_t)(0x01u)) /**< Dynostatic buffer is not initialized. */
#define ERROR_DS_INVALID_ARG         ((ds_err_code_t)(0x02u)) /**< Some given argument is incorrect. */
#define ERROR_DS_ALREADY_INIT        ((ds_err_code_t)(0x03u)) /**< Dynostatic buffer was already initialized and cannot be initialized again. */
#define ERROR_DS_NO_MEMORY           ((ds_err_code_t)(0x04u)) /**< No free memory to allocate in dynostatic_buffer. */
#define ERROR_DS_NO_ALLOCATORS       ((ds_err_code_t)(0x05u)) /**< No free allocators to use. */
#define ERROR_DS_TOO_BIG_CHUNK       ((ds_err_code_t)(0x06u)) /**< Demanded size of chunk is bigger than configured max size. */
#define ERROR_DS_MEMORY_OUT_OF_DS    ((ds_err_code_t)(0x07u)) /**< Pointer given to deallocate is not allocate in dynostatic-buffer. */
#define ERROR_DS_CRITICAL_ERR        ((ds_err_code_t)(0x08u)) /**< Critical error detected. */
#define ERROR_DS_ALLOCATOR_NOT_FOUND ((ds_err_code_t)(0x09u)) /**< Allocator for given pointer is not found. */
#define ERROR_DS_PTR_ALLOC_YET       ((ds_err_code_t)(0x0Au)) /**< Pointer is already allocated. */

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

#ifndef DS_ZERO_ON_FREE        /**< If You not use CMake and KConfig. */
    #define DS_ZERO_ON_FREE 1u /**< Zero freed memory before releasing the block (security/cost trade-off). */
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
 * @brief Lifecycle state of an allocator record (see ds_allocator_t for the
 *        full lifecycle diagram and the invariants tied to these states).
 *
 * @note CONTRACT: DS_NOT_USED must remain 0. ds_initialize_allocation()
 *       establishes the all-records-DS_NOT_USED state by zeroing the
 *       allocators array (ds_zero), and the compact-prefix invariant
 *       assumes a zeroed record reads as DS_NOT_USED. Renumbering this
 *       enum silently breaks initialization.
 */
typedef enum {
    DS_NOT_USED = 0x00, /**< Record carries no block: either never used, or
                             reclaimed by bump-head rollback/cascade. head and
                             size are zero and meaningless. Scans may stop at
                             the first DS_NOT_USED record (compact prefix).
                             Entered from: initialization, rollback of a
                             trailing block. Leaves to: DS_ALLOCATED via a
                             fresh bump allocation. */
    DS_FREE = 0x01,     /**< Block was freed but its record is parked for
                             reuse: head and size (physical capacity) remain
                             valid, and contents are zeroed when
                             DS_ZERO_ON_FREE is enabled. A later request of
                             size <= capacity may reuse this exact block.
                             Entered from: DS_ALLOCATED via ds_free of a
                             non-trailing block. Leaves to: DS_ALLOCATED via
                             reuse, or DS_NOT_USED via the reclamation
                             cascade. */
    DS_ALLOCATED = 0x02 /**< Block is live and owned by the caller: head and
                             size (physical capacity, >= the requested size)
                             are valid, contents belong to the user.
                             Entered from: DS_NOT_USED (bump allocation) or
                             DS_FREE (reuse). Leaves to: DS_FREE or
                             DS_NOT_USED via ds_free, depending on whether
                             the block is trailing. */
} ds_allocator_status_t;

/**
 * @struct ds_allocator_t
 * @brief Bookkeeping record describing one memory block within the buffer.
 *
 * Lifecycle of a record:
 * @verbatim
 *   DS_NOT_USED --(bump allocation)--> DS_ALLOCATED <--(free/reuse)--> DS_FREE
 *        ^                                                               |
 *        +----------------(bump-head rollback / cascade)----------------+
 * @endverbatim
 * A record returns to DS_NOT_USED only when the bump head is rolled back
 * over it (trailing-block reclamation); an ordinary free parks it as DS_FREE
 * so the block can be reused by a later allocation of equal or smaller size.
 *
 * Field semantics depend on allocation_status:
 * - DS_NOT_USED: head and size are zero and carry no meaning (slot never
 *   used, or reclaimed by rollback).
 * - DS_ALLOCATED / DS_FREE: head is the block's offset from the start of
 *   dynostatic_buffer_t::memory; size is the block's PHYSICAL CAPACITY —
 *   the requested size rounded up to DS_ALIGNMENT. Reusing a block with a
 *   smaller request does NOT shrink size: capacity is retained for the
 *   lifetime of the record.
 *
 * Invariants maintained by ds_malloc/ds_free — allocator scans, the early
 * break on DS_NOT_USED, and the reclamation cascade all rely on them:
 * 1. Compact prefix: every record past the first DS_NOT_USED slot is also
 *    DS_NOT_USED, so scans may legally stop at the first DS_NOT_USED entry.
 * 2. Ordering: among non-DS_NOT_USED records, array index order equals head
 *    order (heads strictly increase with index).
 * 3. Gapless partition: non-DS_NOT_USED records tile [0, data_head) exactly:
 *    head[0] == 0 and head[i+1] == head[i] + size[i]. In particular the
 *    block ending at data_head always sits at the highest touched index.
 */
typedef struct {
    size_t head; /**< Offset of the block from the start of dynostatic_buffer_t::memory.
                      Meaningful only when allocation_status != DS_NOT_USED. */
    size_t size; /**< Physical capacity of the block (requested size aligned up to
                      DS_ALIGNMENT); preserved on reuse. Meaningful only when
                      allocation_status != DS_NOT_USED. */

    ds_allocator_status_t allocation_status; /**< Lifecycle state of this record; governs
                                                  the meaning of head and size. */
} ds_allocator_t;

/**
 * @struct dynostatic_buffer_t
 * @brief Self-contained allocator instance emulating dynamic allocation
 *        inside a caller-provided static buffer — no heap, no globals.
 *
 * All state lives inline in this structure, so an instance can be placed in
 * static storage (recommended), on a stack, or inside another object, and
 * multiple independent instances may coexist. Note the footprint: roughly
 * DS_BUFFER_MEMORY_SIZE + DS_MAX_ALLOCATION_COUNT * sizeof(ds_allocator_t)
 * bytes — on small targets prefer static storage duration over the stack.
 *
 * Lifecycle: zero the structure (static storage duration does this for
 * free), then ds_initialize_allocation(), then the allocation API, then
 * ds_deinit_allocation() (zeroes all memory and bookkeeping).
 *
 * @warning Initialization detection relies on init_magic, so the structure
 *          MUST be zero-initialized before the first
 *          ds_initialize_allocation() call. An instance containing garbage
 *          (e.g. a stack variable) may spuriously read as already
 *          initialized. Static storage duration satisfies this requirement
 *          automatically.
 *
 * @warning Not thread-safe and not ISR-safe: no internal locking. The
 *          caller must serialize all API calls on a given instance
 *          (distinct instances are fully independent).
 *
 * Pointers returned by the allocation API point into @ref memory and are
 * aligned to DS_ALIGNMENT. They become invalid after ds_free() of the block
 * and after ds_deinit_allocation() of the instance.
 */
typedef struct {
    uint16_t init_magic;    /**< Equals DS_MAGIC_NUMBER while the instance is
                                 initialized; any other value means
                                 uninitialized. Requires the structure to be
                                 zeroed before first init (see warning). */
    size_t data_head;       /**< Bump pointer: offset of the first byte of
                                 never-touched (or reclaimed) space in memory.
                                 Always in [0, DS_BUFFER_MEMORY_SIZE]. Equal to
                                 the sum of capacities of all non-DS_NOT_USED
                                 records (gapless-partition invariant, see
                                 ds_allocator_t). Rolls back when trailing
                                 blocks are freed. */
    size_t used_allocators; /**< Number of records currently in DS_ALLOCATED
                                 state (live blocks owned by callers). Parked
                                 DS_FREE records are NOT counted. Always in
                                 [0, DS_MAX_ALLOCATION_COUNT]. */

    alignas(DS_ALIGNMENT)
        uint8_t memory[DS_BUFFER_MEMORY_SIZE];          /**< The arena. All user pointers point
                                                             into this array; alignas guarantees the
                                                             base address (and therefore every
                                                             aligned offset) meets DS_ALIGNMENT. */
    ds_allocator_t allocators[DS_MAX_ALLOCATION_COUNT]; /**< Block records; invariants documented
                                                             at ds_allocator_t. Records are
                                                             recruited in index order (compact
                                                             prefix). */
} dynostatic_buffer_t;

/*-----Public-Function-Declaration----*/

/**
 * @brief Initialize allocation buffer
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 *
 * @retval ERROR_DS_OK Dynostatic-buffer is properly initialized.
 * @retval ERROR_DS_INVALID_ARG Some given parameter is incorrect.
 * @retval ERROR_DS_ALREADY_INIT Dynostatic-buffer was already initialized and cannot be initialized again.
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
 * @retval ERROR_DS_NO_ALLOCATORS There is no free allocators when a new block is allocated (previous pointer is NULL).
 * @retval ERROR_DS_TOO_BIG_CHUNK Demanded size of memory block is bigger than configured max size.
 * @retval ERROR_DS_MEMORY_OUT_OF_DS Given pointer is not from dynostatic-buffer when it is freed (size == 0).
 * @retval ERROR_DS_CRITICAL_ERR Resizing an already allocated block is not yet implemented.
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

#endif /* DYNOSTATIC_BUFFER_H */
