.. _configuration:

Configuration
=============

``dynostatic-buffer`` has no runtime configuration: everything is fixed at
compile time through preprocessor macros. This keeps the footprint fully
predictable — the size of an instance, the number of allocations it can hold and
the alignment it guarantees are all known when the binary is linked.

.. important::

   These macros change ``sizeof(dynostatic_buffer_t)`` and the meaning of the
   offsets stored in it. **Every** translation unit that includes
   ``dynostatic-buffer.h`` — the library itself and all of your code — must be
   compiled with identical values. Mismatched values compile cleanly but lead to
   silent memory corruption. Define them once in a shared place: a CMake cache
   entry, the ``DYNOSTATIC_BUFFER_CPPFLAGS`` Make variable, or a
   ``-D`` on every compile line.

Each macro is guarded with ``#ifndef``, so any value you define (via CMake,
KConfig, or ``-D``) before the header is included takes precedence over the
default shown below.

Options
-------

.. c:macro:: DS_BUFFER_MEMORY_SIZE

   *Default:* ``1024``.

   Size in bytes of the arena embedded in each instance — the total pool every
   allocation is carved from. This is the dominant term in the instance's memory
   footprint. Must be at least :c:macro:`DS_MAX_ALLOCATION_SIZE`.

.. c:macro:: DS_MAX_ALLOCATION_COUNT

   *Default:* ``10``.

   Maximum number of allocator records, i.e. the hard cap on how many blocks
   (live *or* parked for reuse) can exist at once. When they are exhausted,
   :c:func:`ds_malloc` fails with :c:macro:`ERROR_DS_NO_ALLOCATORS` regardless of
   how much memory is free. Must be positive.

.. c:macro:: DS_MAX_ALLOCATION_SIZE

   *Default:* ``256``.

   The largest size a single allocation may request. Requests above this cap are
   rejected with :c:macro:`ERROR_DS_TOO_BIG_CHUNK`. Must be positive, a multiple
   of :c:macro:`DS_ALIGNMENT`, and no larger than :c:macro:`DS_BUFFER_MEMORY_SIZE`.

.. c:macro:: DS_ALIGNMENT

   *Default:* ``4``.

   Alignment, in bytes, of every returned pointer and the rounding granularity
   for block capacities. Must be a power of two, at least the alignment of a
   32-bit type, and no larger than the platform's strictest fundamental
   alignment. Larger values waste more to internal rounding but satisfy stricter
   types (e.g. SIMD or DMA buffers).

.. c:macro:: DS_ZERO_ON_FREE

   *Default:* ``0``.

   When set to ``1``, the full capacity of a block is zeroed on :c:func:`ds_free`
   (and when a block is released during a relocating :c:func:`ds_realloc`).
   Useful for scrubbing sensitive data or getting deterministic contents, at the
   cost of the zeroing work. Note this does **not** affect :c:func:`ds_calloc`,
   whose zero-fill is unconditional.

.. c:macro:: DS_LOG_ENABLE

   *Default:* ``0``.

   When set to ``1``, enables the library's internal logging. Leave it off unless
   you are debugging the allocator itself.

Compile-time validation
------------------------

The header turns the constraints above into ``_Static_assert`` (C11) /
``static_assert`` (C++) checks, so an inconsistent configuration fails to
**compile** rather than misbehaving at runtime. The enforced invariants are:

* ``DS_ALIGNMENT`` is a power of two.
* ``DS_ALIGNMENT`` is at least the alignment required by 32-bit types.
* ``DS_ALIGNMENT`` does not exceed the strictest fundamental alignment on the
  platform.
* ``DS_MAX_ALLOCATION_SIZE <= DS_BUFFER_MEMORY_SIZE``.
* ``DS_MAX_ALLOCATION_COUNT`` and ``DS_MAX_ALLOCATION_SIZE`` are both positive.
* ``DS_MAX_ALLOCATION_SIZE`` is a whole multiple of ``DS_ALIGNMENT``.
* ``DS_MAX_ALLOCATION_SIZE`` is small enough that the internal align-up
  arithmetic cannot overflow ``size_t``.

Sizing an instance
------------------

An instance costs approximately::

   DS_BUFFER_MEMORY_SIZE  +  DS_MAX_ALLOCATION_COUNT * sizeof(ds_allocator_t)

bytes (plus a small fixed header and any padding). ``sizeof(ds_allocator_t)`` is
two ``size_t`` fields and an enum, so on a 32-bit target each record is on the
order of a dozen bytes. Two forces pull in opposite directions:

* **Raising** ``DS_MAX_ALLOCATION_COUNT`` lets you hold more concurrent blocks
  but grows every instance, even ones that never use all the slots.
* **Lowering** ``DS_ALIGNMENT`` reduces the bytes lost to per-block rounding but
  may be too weak for types with strict alignment needs.

Because the whole footprint is compile-time constant, you can size it against a
concrete budget and be confident it will not grow at runtime.
