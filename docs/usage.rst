Usage guide
===========

A task-oriented tour of the API. Every snippet assumes an initialized instance
(see :doc:`getting-started`) and, for brevity, omits some error checks that
production code should keep. The full, compilable versions live in the
``examples/`` directory.

The calling convention
----------------------

A few conventions run through the whole API and are worth internalizing once:

* **Everything returns an error code.** Functions return a
  :c:type:`ds_err_code_t`; :c:macro:`ERROR_DS_OK` (which is ``0``) means
  success. Nothing is returned through the function value except status.
* **Pointers are passed by address.** Allocating and freeing functions take a
  ``void **`` so they can both read your current pointer (as a safety guard)
  and write the new one back. On success :c:func:`ds_malloc` writes the block
  address into ``*p_memory``; :c:func:`ds_free` writes ``NULL``.
* **On failure, outputs are left untouched.** If a call returns anything other
  than ``ERROR_DS_OK``, your pointer and the instance state are unchanged. That
  makes it safe to branch on the error without cleaning up half-done work.

A robust way to handle the return code is a small helper macro:

.. code-block:: c

   #define CHECK(expr)                                            \
       do {                                                       \
           ds_err_code_t err_ = (expr);                           \
           if (err_ != ERROR_DS_OK) {                             \
               fprintf(stderr, "%s -> %u\n", #expr, (unsigned)err_); \
               return EXIT_FAILURE;                               \
           }                                                      \
       } while (0)

Allocating and freeing
----------------------

:c:func:`ds_malloc` allocates at least ``size`` bytes and writes the block
address into ``*p_memory``. The input pointer must be ``NULL`` (or a valid prior
pointer you are done with) — never uninitialized — because the function reads it
to reject overwriting a live block:

.. code-block:: c

   int *numbers = NULL;
   CHECK(ds_malloc(&ds_buffer, (void **)&numbers, 4 * sizeof(int)));
   /* ... use numbers[0..3] ... */
   CHECK(ds_free(&ds_buffer, (void **)&numbers));
   /* numbers is now NULL again */

:c:func:`ds_free` accepts only a pointer to the **start** of a live block.
Interior pointers, foreign pointers and double frees are rejected
(:c:macro:`ERROR_DS_ALLOCATOR_NOT_FOUND` or
:c:macro:`ERROR_DS_MEMORY_OUT_OF_DS`) without corrupting anything. On success it
NULLs your pointer, which is what makes reusing the same variable safe.

Zero-initialized arrays with ``ds_calloc``
------------------------------------------

:c:func:`ds_calloc` allocates ``len * size_of_elem`` bytes and zero-fills
them. The multiplication is overflow-checked, and the zeroing is
**unconditional** — it happens regardless of the :c:macro:`DS_ZERO_ON_FREE`
setting.

.. code-block:: c

   int *values = NULL;
   CHECK(ds_calloc(&ds_buffer, (void **)&values, 4, sizeof(int)));
   /* values[0..3] are guaranteed 0 */

Resizing with ``ds_realloc``
----------------------------

:c:func:`ds_realloc` resizes a block, preserving its contents up to the
smaller of the old and new sizes. It follows C ``realloc`` semantics with a few
deliberate, safer deviations:

* ``size == 0`` behaves like :c:func:`ds_free`.
* ``*p_memory == NULL`` behaves like :c:func:`ds_malloc`.
* **Shrinking** happens in place, the pointer is unchanged, and the original
  capacity is retained (occupancy does not drop).
* **Growing the most recently placed block** extends it in place when the
  trailing space allows — no copy, pointer unchanged.
* **Growing otherwise** allocates a new block, copies the contents, and frees
  the old one; the pointer changes. This transiently needs a spare allocator
  record for the old+new pair.
* A pointer that is not the start of a live block is **rejected**, unlike C
  ``realloc``'s undefined behaviour.

Always assign the result back through the same pointer, because in the general
case the block moves:

.. code-block:: c

   /* Grow a 4-int array to 8 ints, preserving the first four. */
   CHECK(ds_realloc(&ds_buffer, (void **)&values, 8 * sizeof(int)));
   for (int i = 4; i < 8; ++i) {
       values[i] = i + 1;      /* bytes beyond the old size are indeterminate */
   }

On any failure the original block and pointer are left fully intact, so the
common ``p = realloc(p, ...)`` footgun (losing the only pointer to the old
block on failure) does not apply here — but assigning back is still the right
habit.

Inspecting an instance
----------------------

Three read-only getters let you reason about capacity before committing to an
allocation. Each returns a **snapshot** that is only valid until the next
mutating call.

* :c:func:`ds_get_memory_usage` — arena occupancy as a ``0..100`` percentage,
  summing physical capacities of live blocks.
* :c:func:`ds_get_free_allocator_cnt` — allocator slots not holding a live
  block. This is an *upper bound* on further concurrent allocations, not a
  guarantee any specific one will succeed.
* :c:func:`ds_get_max_new_allocation_size` — the largest ``size`` for which an
  immediate :c:func:`ds_malloc` would succeed right now, considering both
  reuse and fresh bump space.

.. code-block:: c

   uint8_t usage = 0;
   size_t free_slots = 0, max_alloc = 0;
   CHECK(ds_get_memory_usage(&ds_buffer, &usage));
   CHECK(ds_get_free_allocator_cnt(&ds_buffer, &free_slots));
   CHECK(ds_get_max_new_allocation_size(&ds_buffer, &max_alloc));
   printf("usage=%u%%  free_slots=%zu  max_new_alloc=%zu\n",
          usage, free_slots, max_alloc);

A ``max_new_alloc`` of ``0`` means nothing can be allocated at any size; use the
other two getters to tell whether it is the memory or the allocator slots that
are exhausted.

Bounds-checked writes
---------------------

:c:func:`ds_safe_memory_set` and :c:func:`ds_safe_memory_copy` are guarded
alternatives to ``memset``/``memcpy``. Before writing, they verify that the
destination is part of a live allocation in the instance and that the allocation
is large enough. An over-long write is rejected with
:c:macro:`ERROR_DS_NO_MEMORY` and the destination is left untouched — a cheap
guard against the stray writes that are so painful to debug on embedded targets.

.. code-block:: c

   char *text = NULL;
   CHECK(ds_malloc(&ds_buffer, (void **)&text, 16));

   CHECK(ds_safe_memory_set(&ds_buffer, text, 0, 16));         /* clear safely */
   CHECK(ds_safe_memory_copy(&ds_buffer, text, "hello", 6));   /* incl. the NUL */

   /* This would overrun the 16-byte block, so it is refused and `text` is
    * left intact instead of being corrupted. */
   ds_err_code_t err = ds_safe_memory_copy(&ds_buffer, text,
       "this string is definitely longer than sixteen bytes", 52);
   /* err == ERROR_DS_NO_MEMORY */

Putting it together
-------------------

The four ideas above compose into the typical pattern for a constrained system:
initialize once into static storage, allocate with the pointer-by-address
convention, check every error code, optionally introspect before large
requests, and tear down with :c:func:`ds_deinit_allocation`. For the exact
semantics of every parameter and return value, continue to the
:doc:`api` reference.
