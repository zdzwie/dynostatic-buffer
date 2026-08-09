Design overview
===============

This page explains how ``dynostatic-buffer`` works underneath the API so that
you can predict its behaviour — especially its memory footprint, its failure
modes and when a request will or will not succeed. If you just want to get code
running, jump to :doc:`getting-started` and come back here when you need the
details.

The mental model
----------------

An instance is a single ``dynostatic_buffer_t`` structure containing two things:

* **The arena** — a byte array ``memory[DS_BUFFER_MEMORY_SIZE]`` from which all
  user pointers are carved. It is aligned so that every offset the allocator
  hands out satisfies :c:macro:`DS_ALIGNMENT`.
* **The allocator records** — a fixed array
  ``allocators[DS_MAX_ALLOCATION_COUNT]`` of bookkeeping entries. Each record
  describes one block: where it starts in the arena, its physical capacity, and
  its lifecycle state.

Because both arrays are inline, an instance is entirely self-contained. There
is no hidden per-block header stored in the arena and no pointer chasing: the
records live beside the data, not inside it.

.. note::

   The instance is comparatively large — roughly
   ``DS_BUFFER_MEMORY_SIZE + DS_MAX_ALLOCATION_COUNT * sizeof(ds_allocator_t)``
   bytes. On small targets, give it **static storage duration** rather than
   putting it on a stack. Static storage also zero-initializes it for free,
   which is required before the first :c:func:`ds_initialize_allocation`
   call.

Bump allocation
---------------

Fresh allocations are served by a **bump pointer**, ``data_head``, which marks
the first byte of never-touched space. A new allocation of ``n`` bytes rounds
``n`` up to :c:macro:`DS_ALIGNMENT`, claims the region
``[data_head, data_head + aligned_n)``, records it, and advances ``data_head``.
This is why allocation is cheap and its cost does not depend on how much is
already allocated.

The bump pointer only advances for genuinely new space. Two things can move it
back or avoid moving it at all:

* **Reuse.** When a block is freed in the middle of the arena, its record is
  *parked* (state ``DS_FREE``) instead of discarded. A later request that fits
  within that block's retained capacity reuses it in place — no bump, no move.
* **Reclamation.** When the *trailing* block (the one ending exactly at
  ``data_head``) is freed, ``data_head`` rolls back over it, returning the space
  to the general pool. The rollback cascades through any adjacent already-freed
  blocks, so freeing in last-in-first-out order fully defragments the tail.

The allocator record lifecycle
-------------------------------

Every block record moves through three states:

``DS_NOT_USED``
   The record holds no block — either it was never used or it was reclaimed by
   a bump-head rollback. Its ``head`` and ``size`` are meaningless.

``DS_ALLOCATED``
   The block is live and owned by the caller. ``head`` is its offset in the
   arena and ``size`` is its **physical capacity** (the requested size rounded
   up to :c:macro:`DS_ALIGNMENT`).

``DS_FREE``
   The block was freed but its record is parked for reuse. ``head`` and ``size``
   remain valid; a later request of ``size <= capacity`` may reuse it.

.. code-block:: text

     DS_NOT_USED --(bump allocation)--> DS_ALLOCATED <--(free / reuse)--> DS_FREE
          ^                                                                 |
          +------------------(bump-head rollback / cascade)-----------------+

Records are recruited in index order and the array is kept as a **compact
prefix**: everything past the first ``DS_NOT_USED`` slot is also
``DS_NOT_USED``. Among used records, array order matches address order, and the
used records tile ``[0, data_head)`` with no gaps. These invariants are what let
the getters and the reclamation cascade work in a simple linear scan; see the
struct documentation in the :doc:`api` reference for the precise contract.

Capacity, not requested size
----------------------------

A block's ``size`` is its *capacity* — the requested size rounded up to the
alignment — and that capacity is **retained for the life of the record**.
Concretely:

* Reusing a parked ``DS_FREE`` block for a smaller request does not shrink it.
* Shrinking a block with :c:func:`ds_realloc` keeps the original capacity.

This matters when you read :c:func:`ds_get_memory_usage`: it sums physical
capacities, so the reported occupancy can be larger than the total of the sizes
you asked for. It is the honest number for "how much of the arena is currently
spoken for."

Alignment
---------

All returned pointers are aligned to :c:macro:`DS_ALIGNMENT` (default 4 bytes).
The arena base is over-aligned with ``alignas`` so that every aligned offset is
also correctly aligned in absolute terms. Compile-time assertions enforce that
``DS_ALIGNMENT`` is a power of two, at least large enough for 32-bit types, and
no larger than the strictest fundamental alignment on the platform. See
:doc:`configuration` for how to change it and what the trade-offs are.

Safety guarantees
-----------------

Unlike raw ``malloc``/``free``, the API validates its inputs and reports
problems as :doc:`error codes <error-codes>` instead of invoking undefined
behaviour:

* Freeing an interior pointer, a foreign pointer, or an already-freed block is
  rejected without touching any state.
* :c:func:`ds_malloc` refuses to overwrite a pointer that already refers to a
  live block, catching a class of leaks (it reads ``*p_memory`` as a guard, so
  that argument must be ``NULL`` or a valid prior pointer, never uninitialized).
* :c:func:`ds_safe_memory_set` and :c:func:`ds_safe_memory_copy` verify that
  the destination belongs to a live allocation and is large enough before
  writing a single byte.

What it does **not** do
-----------------------

* **No thread safety.** There is no internal locking. Serialize all calls on a
  given instance yourself; distinct instances are fully independent, so
  per-thread instances are a valid pattern. It is likewise not ISR-safe.
* **No general defragmentation.** Only the *trailing* freed region is reclaimed.
  Interior fragmentation is mitigated by reuse but not eliminated; an allocation
  pattern that frees middle blocks and then requests larger ones can fail with
  :c:macro:`ERROR_DS_NO_MEMORY` even though the total free bytes would suffice.
* **No growth.** The arena size is fixed at compile time. There is no fallback
  to a system heap.
