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

#ifndef DS_LOG_ENABLE        /**< If You not use CMake and KConfig. */
    #define DS_LOG_ENABLE 0u /**< Enable or disable logging from dynostatic-buffer. */
#endif

#ifndef DS_ZERO_ON_FREE        /**< If You not use CMake and KConfig. */
    #define DS_ZERO_ON_FREE 0u /**< Zero the contents of freed blocks in dynostatic-buffer. */
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
 * @brief Initialize an allocator instance over its embedded arena.
 *
 * Zeroes the arena and all allocator records, resets the bump head, and
 * arms the init_magic marker. After success the full DS_BUFFER_MEMORY_SIZE
 * is allocatable.
 *
 * @pre The structure must be zero-initialized before the FIRST call (see
 *      the @warning at dynostatic_buffer_t) — garbage content may spuriously
 *      read as already initialized.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 *
 * @retval ERROR_DS_OK Instance initialized; the whole arena is available.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer is NULL.
 * @retval ERROR_DS_ALREADY_INIT Instance is already initialized; call
 *                               ds_deinit_allocation() first to reset it.
 */
ds_err_code_t ds_initialize_allocation(dynostatic_buffer_t *p_ds_buffer);

/**
 * @brief Allocate a memory block of at least @p size bytes.
 *
 * The returned pointer is aligned to DS_ALIGNMENT and the block's physical
 * capacity is @p size rounded up to DS_ALIGNMENT. The request is served
 * either by reusing a previously freed block of sufficient capacity or by
 * a fresh allocation from untouched space. On success *p_memory receives
 * the block address; on any failure *p_memory is left unchanged.
 *
 * @pre *p_memory must be initialized before the call — NULL or a previously
 *      obtained pointer. The function reads it to reject overwriting a
 *      pointer to a live block (a leak guard); passing an uninitialized
 *      value is undefined behaviour and may yield spurious
 *      ERROR_DS_PTR_ALLOC_YET.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in, out] p_memory In: must not point to a live block. Out: address
 *                          of the allocated block.
 * @param[in] size Requested size in bytes (1..DS_MAX_ALLOCATION_SIZE).
 *
 * @retval ERROR_DS_OK Block allocated; *p_memory points to it.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer or p_memory is NULL, or size is 0.
 * @retval ERROR_DS_TOO_BIG_CHUNK size exceeds DS_MAX_ALLOCATION_SIZE.
 * @retval ERROR_DS_PTR_ALLOC_YET *p_memory already points to a live block;
 *                                free it first or use ds_realloc().
 * @retval ERROR_DS_NO_ALLOCATORS All allocator records are occupied.
 * @retval ERROR_DS_NO_MEMORY No free region can satisfy the aligned size.
 */
ds_err_code_t ds_malloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size);

/**
 * @brief Free a block previously obtained from this instance.
 *
 * Only a pointer to the START of a live block is accepted — interior
 * pointers, foreign pointers, and already-freed addresses are rejected
 * without touching any state. When DS_ZERO_ON_FREE is enabled (default),
 * the block's full capacity is zeroed before release. A freed trailing
 * block is reclaimed immediately (including a cascade through adjacent
 * freed blocks), making the space available to allocations of any size;
 * a freed interior block is parked for reuse by requests fitting its
 * capacity. On success *p_memory is set to NULL.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in, out] p_memory In: address of a live block. Out: NULL on success,
 *                          unchanged on failure.
 *
 * @retval ERROR_DS_OK Block released; *p_memory is now NULL.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer, p_memory or *p_memory is NULL.
 * @retval ERROR_DS_MEMORY_OUT_OF_DS *p_memory lies outside this instance's
 *                                   arena.
 * @retval ERROR_DS_ALLOCATOR_NOT_FOUND *p_memory is inside the arena but is
 *                                      not the start of a live block —
 *                                      typically a double free or an
 *                                      interior pointer.
 */
ds_err_code_t ds_free(dynostatic_buffer_t *p_ds_buffer, void **p_memory);

/**
 * @brief Allocate a zero-filled array of @p len elements of @p size_of_elem
 *        bytes each.
 *
 * Equivalent to ds_malloc(len * size_of_elem) followed by zeroing the
 * requested bytes. The zero fill is unconditional — it does NOT depend on
 * DS_ZERO_ON_FREE. The multiplication is checked for overflow. Inherits
 * the ds_malloc() precondition on *p_memory (see ERROR_DS_PTR_ALLOC_YET).
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in, out] p_memory In: must not point to a live block. Out: address
 *                          of the zeroed array.
 * @param[in] len Number of elements; must not be 0.
 * @param[in] size_of_elem Size of one element in bytes; must not be 0.
 *
 * @retval ERROR_DS_OK Array allocated and zeroed.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG NULL arguments, zero len/size_of_elem, or
 *                              len * size_of_elem overflows size_t.
 * @retval ERROR_DS_TOO_BIG_CHUNK Total size exceeds DS_MAX_ALLOCATION_SIZE.
 * @retval ERROR_DS_PTR_ALLOC_YET *p_memory already points to a live block.
 * @retval ERROR_DS_NO_ALLOCATORS All allocator records are occupied.
 * @retval ERROR_DS_NO_MEMORY No free region can satisfy the aligned size.
 */
ds_err_code_t ds_calloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t len, size_t size_of_elem);

/**
 * @brief Resize a block, preserving its contents up to the smaller of the
 *        old and new sizes.
 *
 * Semantics follow C realloc with explicit deviations:
 * - size == 0: equivalent to ds_free(p_memory).
 * - *p_memory == NULL: equivalent to ds_malloc(size).
 * - Shrink (new aligned size fits the block's capacity): in place, pointer
 *   unchanged, capacity RETAINED (no split; ds_get_memory_usage() keeps
 *   reporting the original capacity).
 * - Grow of the most recently placed block: extended in place when space
 *   allows — pointer unchanged, no copy.
 * - Grow otherwise: a new block is allocated, contents are copied, the old
 *   block is freed (zeroed under DS_ZERO_ON_FREE). Requires a free
 *   allocator record for the transient old+new pair. Bytes beyond the old
 *   size have indeterminate values.
 * - A pointer that is not the start of a live block is REJECTED — unlike
 *   C realloc's undefined behaviour, and no new block is allocated.
 *
 * On any failure the original block and *p_memory are left fully intact.
 *
 * @param[in, out] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[in, out] p_memory In: NULL or address of a live block. Out: address
 *                          of the resized block (may differ from the input),
 *                          NULL after size == 0, unchanged on failure.
 * @param[in] size New size in bytes (0..DS_MAX_ALLOCATION_SIZE).
 *
 * @retval ERROR_DS_OK Operation completed as described above.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer or p_memory is NULL (or, for
 *                              size == 0, *p_memory is NULL).
 * @retval ERROR_DS_TOO_BIG_CHUNK size exceeds DS_MAX_ALLOCATION_SIZE.
 * @retval ERROR_DS_MEMORY_OUT_OF_DS *p_memory lies outside this instance's
 *                                   arena.
 * @retval ERROR_DS_ALLOCATOR_NOT_FOUND *p_memory is not the start of a live
 *                                      block (double free / interior pointer).
 * @retval ERROR_DS_NO_ALLOCATORS No free record for the moved block.
 * @retval ERROR_DS_NO_MEMORY No region can hold the grown block.
 */
ds_err_code_t ds_realloc(dynostatic_buffer_t *p_ds_buffer, void **p_memory, size_t size);

/**
 * @brief Get the arena occupancy as a percentage (0..100, rounded down).
 *
 * Sums the PHYSICAL CAPACITIES of live blocks — sizes rounded up to
 * DS_ALIGNMENT and retained across reuse and shrinking — so the result may
 * exceed the sum of requested sizes. Parked freed blocks count as free.
 * 0 means no live blocks; 100 means the arena is fully occupied by live
 * blocks.
 *
 * @note Snapshot semantics: valid only until the next mutating call.
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_memory_usage Occupancy percentage (0..100).
 *
 * @retval ERROR_DS_OK Value successfully computed and written.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer or p_memory_usage is NULL.
 * @retval ERROR_DS_CRITICAL_ERR Internal state corrupted: live capacities
 *                               exceed the arena size (writes 0).
 */
ds_err_code_t ds_get_memory_usage(const dynostatic_buffer_t *p_ds_buffer, uint8_t *p_memory_usage);

/**
 * @brief Get the largest allocation size that would succeed right now.
 *
 * Returns the maximal @p size for which an immediate ds_malloc() call on
 * this instance would return ERROR_DS_OK. Both allocation paths are
 * considered: reuse of the largest parked DS_FREE block and a fresh bump
 * allocation (clamped to DS_MAX_ALLOCATION_SIZE and to the aligned usable
 * remainder — an unaligned buffer tail is excluded as unallocatable).
 *
 * A result of 0 means no allocation of any size can currently succeed —
 * either the memory or the allocator slots are exhausted; use
 * ds_get_free_allocator_cnt() and ds_get_memory_usage() to tell which.
 *
 * @note The value is a snapshot: it is guaranteed only until the next call
 *       that mutates this instance (ds_malloc/ds_calloc/ds_realloc/ds_free).
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_max_new_allocation Largest currently satisfiable allocation
 *                                  size in bytes; 0 when nothing can be
 *                                  allocated.
 *
 * @retval ERROR_DS_OK Value successfully computed and written.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer or p_max_new_allocation is NULL.
 */
ds_err_code_t ds_get_max_new_allocation_size(const dynostatic_buffer_t *p_ds_buffer, size_t *p_max_new_allocation);

/**
 * @brief Get the number of allocator slots not holding a live block.
 *
 * Counts slots in DS_NOT_USED or DS_FREE state — an UPPER BOUND on how many
 * additional concurrent allocations this instance can hold, NOT a guarantee
 * that any particular allocation will succeed: parked DS_FREE slots serve
 * only requests fitting their retained capacity, and every allocation is
 * further subject to available memory (see
 * ds_get_max_new_allocation_size() for a success-oriented view).
 *
 * A result of 0 means ds_malloc() is guaranteed to fail with
 * ERROR_DS_NO_ALLOCATORS regardless of the requested size.
 *
 * @note The value is a snapshot: it is guaranteed only until the next call
 *       that mutates this instance.
 *
 * @param[in] p_ds_buffer Pointer to dynostatic-buffer structure.
 * @param[out] p_free_allocators Number of slots available for new
 *                               allocations (0..DS_MAX_ALLOCATION_COUNT).
 *
 * @retval ERROR_DS_OK Value successfully computed and written.
 * @retval ERROR_DS_NO_INIT Dynostatic-buffer is not initialized.
 * @retval ERROR_DS_INVALID_ARG p_ds_buffer or p_free_allocators is NULL.
 */
ds_err_code_t ds_get_free_allocator_cnt(const dynostatic_buffer_t *p_ds_buffer, size_t *p_free_allocators);

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
