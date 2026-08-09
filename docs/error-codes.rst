Error codes
===========

Every public function returns a :c:type:`ds_err_code_t`. The value
:c:macro:`ERROR_DS_OK` is ``0`` and means success; any non-zero value is a
specific, actionable failure. This page is a reference for what each code means
and how to react to it.

.. tip::

   Because :c:macro:`ERROR_DS_OK` is ``0``, the idiomatic check is
   ``if (err != ERROR_DS_OK)`` (or simply ``if (err)`` for "did anything go
   wrong"). On failure the function leaves its outputs and the instance state
   untouched, so you can handle the error without undoing partial work.

Reference
---------

.. c:macro:: ERROR_DS_OK

   The operation succeeded and any output parameter has been written. Numeric
   value ``0``; this is the only non-error value.

.. c:macro:: ERROR_DS_NO_INIT

   The instance has not been initialized (or has been deinitialized). Call
   :c:func:`ds_initialize_allocation` first. Also check that the structure was
   zero-initialized before that first call.

.. c:macro:: ERROR_DS_INVALID_ARG

   An argument is invalid: a ``NULL`` pointer where one is required, a zero
   ``size``/``len``/``size_of_elem``, or an overflowing element-count product in
   :c:func:`ds_calloc`. Fix the call site — this indicates a programming error,
   not a resource shortage.

.. c:macro:: ERROR_DS_ALREADY_INIT

   :c:func:`ds_initialize_allocation` was called on an instance that is already
   initialized. Deinitialize it first if you really want to reset it. (Often a
   symptom of an uninitialized, garbage-filled instance — see the
   zero-initialization warning in :doc:`getting-started`.)

.. c:macro:: ERROR_DS_NO_MEMORY

   There is no free region large enough to satisfy the aligned request, even
   though allocator slots may be available. Free something, request less, or use
   :c:func:`ds_get_max_new_allocation_size` to learn the largest size that would
   currently succeed. Also returned by the safe-memory helpers when the
   destination allocation is too small for the write.

.. c:macro:: ERROR_DS_NO_ALLOCATORS

   Every allocator record is occupied (live or parked for reuse), so no new block
   can be tracked regardless of free memory. Free a block, or raise
   :c:macro:`DS_MAX_ALLOCATION_COUNT`. Check with
   :c:func:`ds_get_free_allocator_cnt`.

.. c:macro:: ERROR_DS_TOO_BIG_CHUNK

   The requested size exceeds :c:macro:`DS_MAX_ALLOCATION_SIZE`. Split the
   request, or raise the compile-time cap.

.. c:macro:: ERROR_DS_MEMORY_OUT_OF_DS

   The pointer passed to :c:func:`ds_free`, :c:func:`ds_realloc` or a safe-memory
   helper lies outside this instance's arena — for example a stack/heap pointer,
   or a pointer from a *different* instance. Make sure you are freeing against the
   instance that allocated it.

.. c:macro:: ERROR_DS_ALLOCATOR_NOT_FOUND

   The pointer is inside the arena but is not the start of a live block.
   Typically a double free or an interior pointer (an address partway into a
   block). Only pass back the exact pointer the allocator returned.

.. c:macro:: ERROR_DS_PTR_ALLOC_YET

   The ``*p_memory`` handed to :c:func:`ds_malloc` / :c:func:`ds_calloc` already
   points to a live block. This is the leak guard: free the block first, or use
   :c:func:`ds_realloc` to resize it. Always pass a ``NULL`` (or freed-to-``NULL``)
   pointer into an allocation call.

.. c:macro:: ERROR_DS_CRITICAL_ERR

   An internal invariant was violated — for instance, live capacities summing to
   more than the arena size in :c:func:`ds_get_memory_usage`. This should not
   happen in correct use; it points to memory corruption or a bug and warrants
   investigation rather than a retry.

Which functions return what
---------------------------

The per-function ``@retval`` lists in the :doc:`api` reference are the
authoritative source for exactly which codes a given call can return and under
what conditions. Use this page for the general meaning and the recommended
reaction; use the API reference when you need the precise contract of one
function.
