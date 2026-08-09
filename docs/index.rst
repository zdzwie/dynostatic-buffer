dynostatic-buffer
=================

Dynamic memory allocation served out of a fixed, statically declared buffer.

``dynostatic-buffer`` gives you the ergonomics of
``malloc``/``calloc``/``realloc``/``free`` without a system heap. Every byte it
hands out comes from an arena embedded directly inside a
:c:struct:`dynostatic_buffer_t` instance, so allocation touches no global
state, calls no operating-system services, and can be reasoned about
statically. That makes it a good fit for:

* **Bare-metal and RTOS firmware** where ``malloc`` is unavailable, forbidden
  by a coding standard (MISRA and friends), or simply too unpredictable.
* **Safety- and certification-oriented code** that must bound its worst-case
  memory footprint at build time.
* **Deterministic subsystems** — parsers, protocol stacks, command buffers —
  that want dynamic-looking allocation with a hard, known upper limit.

Highlights
----------

* **No heap, no globals.** All state lives inside the instance; place it in
  static storage, on a stack, or inside another object. Multiple independent
  instances happily coexist.
* **Bounded and configurable.** The arena size, the maximum number of live
  allocations, the per-allocation cap and the alignment are all fixed at
  compile time (see :doc:`configuration`).
* **Safer than raw ``malloc``.** Double frees, interior-pointer frees, foreign
  pointers and leaking overwrites are detected and reported as error codes
  rather than corrupting memory. Optional bounds-checked writes
  (:c:func:`ds_safe_memory_set`, :c:func:`ds_safe_memory_copy`) guard
  against buffer overruns.
* **Introspectable.** Query live occupancy, free allocator slots and the
  largest currently satisfiable request before you commit to an allocation.

Quick taste
-----------

.. code-block:: c

   #include "dynostatic-buffer.h"

   static dynostatic_buffer_t ds_buffer;   /* zero-initialized static storage */

   int main(void)
   {
       ds_initialize_allocation(&ds_buffer);

       int *numbers = NULL;                 /* MUST be NULL going in */
       ds_malloc(&ds_buffer, (void **)&numbers, 4 * sizeof(int));

       for (int i = 0; i < 4; ++i) {
           numbers[i] = (i + 1) * 10;
       }

       ds_free(&ds_buffer, (void **)&numbers);   /* sets numbers back to NULL */
       ds_deinit_allocation(&ds_buffer);
       return 0;
   }

Read :doc:`getting-started` for the same program with proper error handling and
build instructions, and :doc:`overview` for how the allocator works underneath.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   getting-started
   usage
   configuration
   error-codes
   api

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
