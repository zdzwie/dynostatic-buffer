API reference
=============

The complete public API is declared in ``dynostatic-buffer.h``. The entries
below are generated from the Doxygen comments in the source via Breathe, so they
stay in lock-step with the code. For prose introductions to these functions see
:doc:`usage`; for the meaning of the return codes see :doc:`error-codes`.

The API divides into five groups:

* :ref:`lifecycle <api-lifecycle>` — set an instance up and tear it down.
* :ref:`allocation <api-allocation>` — obtain, resize and release blocks.
* :ref:`introspection <api-introspection>` — read-only capacity queries.
* :ref:`safe memory operations <api-safe-memory>` — bounds-checked writes.
* :ref:`types and constants <api-types>` — the structures, enum and macros.

.. _api-lifecycle:

Lifecycle
---------

Initialize an instance before use and deinitialize it when done. See the
lifecycle discussion in :doc:`getting-started`.

.. doxygenfunction:: ds_initialize_allocation
   :project: dynostatic-buffer

.. doxygenfunction:: ds_deinit_allocation
   :project: dynostatic-buffer

.. _api-allocation:

Allocation
----------

The core ``malloc``/``calloc``/``realloc``/``free`` family. All take the target
pointer by address (``void **``): they read it as a safety guard and write the
result back. See :doc:`usage` for the calling convention.

.. doxygenfunction:: ds_malloc
   :project: dynostatic-buffer

.. doxygenfunction:: ds_calloc
   :project: dynostatic-buffer

.. doxygenfunction:: ds_realloc
   :project: dynostatic-buffer

.. doxygenfunction:: ds_free
   :project: dynostatic-buffer

.. _api-introspection:

Introspection
-------------

Read-only getters that report a snapshot of the instance's state, valid only
until the next mutating call.

.. doxygenfunction:: ds_get_memory_usage
   :project: dynostatic-buffer

.. doxygenfunction:: ds_get_free_allocator_cnt
   :project: dynostatic-buffer

.. doxygenfunction:: ds_get_max_new_allocation_size
   :project: dynostatic-buffer

.. _api-safe-memory:

Safe memory operations
----------------------

Bounds-checked alternatives to ``memset``/``memcpy`` that validate the
destination belongs to a live allocation and is large enough before writing.

.. doxygenfunction:: ds_safe_memory_set
   :project: dynostatic-buffer

.. doxygenfunction:: ds_safe_memory_copy
   :project: dynostatic-buffer

.. _api-types:

Types and constants
-------------------

The instance structure, the internal bookkeeping types and the error-code
constants. Most users only interact with :c:struct:`dynostatic_buffer_t`
directly (to declare an instance); the allocator record and its status enum are
documented because they define the invariants that explain the allocator's
behaviour (see :doc:`overview`).

.. doxygenstruct:: dynostatic_buffer_t
   :project: dynostatic-buffer
   :members:

.. doxygenstruct:: ds_allocator_t
   :project: dynostatic-buffer
   :members:

.. doxygenenum:: ds_allocator_status_t
   :project: dynostatic-buffer

.. doxygentypedef:: ds_err_code_t
   :project: dynostatic-buffer

The error-code constants (``ERROR_DS_OK`` and friends) and the compile-time
configuration macros are documented on their own pages: see :doc:`error-codes`
and :doc:`configuration`.
