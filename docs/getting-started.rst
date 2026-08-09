Getting started
===============

This page takes you from an empty source file to a working program, then shows
how to build the library into your own project.

Adding the library to your build
---------------------------------

``dynostatic-buffer`` is a single translation unit
(``library/dynostatic-buffer.c``) plus one public header
(``library/dynostatic-buffer.h``). You can consume it with any of the three
build systems the project ships with.

.. important::

   The :ref:`configuration macros <configuration>` (``DS_BUFFER_MEMORY_SIZE``,
   ``DS_MAX_ALLOCATION_COUNT``, ``DS_ALIGNMENT``, …) change
   ``sizeof(dynostatic_buffer_t)``. **Every** translation unit that includes the
   header — the library and all of your own code — must be compiled with the
   same values, or the layout will disagree and you will get silent memory
   corruption. Set them once, in a place every consumer sees.

CMake (primary)
~~~~~~~~~~~~~~~

.. code-block:: sh

   cmake --preset release
   cmake --build --preset release

Make
~~~~

The top-level ``Makefile`` builds a static and shared library. The build
settings also live in a relocatable fragment,
``library/dynostatic-buffer.mk``, that another project can ``include`` to reuse
the exact compile flags:

.. code-block:: make

   include third_party/dynostatic-buffer/library/dynostatic-buffer.mk

   app.o: app.c
   	$(CC) $(DYNOSTATIC_BUFFER_CPPFLAGS) -c $< -o $@

   app: app.o $(DYNOSTATIC_BUFFER_A)
   	$(CC) $^ -o $@

Everything is namespaced with the ``DYNOSTATIC_BUFFER_`` / ``DS_`` prefixes to
avoid clashing with the host project.

Bazel
~~~~~

.. code-block:: sh

   bazel build //...
   bazel test //...
   bazel run //examples:basic_example

See the top-level ``README`` for the full build matrix.

The lifecycle
-------------

Every instance follows the same four-phase lifecycle:

#. **Zero-initialize** the structure. Static storage duration does this for
   free and is the recommended placement.
#. **Initialize** it once with :c:func:`ds_initialize_allocation`.
#. **Use** the allocation and query API as much as you like.
#. **Deinitialize** with :c:func:`ds_deinit_allocation`, which zeroes all
   memory and bookkeeping. The instance can be initialized again afterwards.

.. warning::

   Initialization is detected via an in-band magic marker, so the structure
   **must** be zeroed before the first :c:func:`ds_initialize_allocation`.
   A stack instance full of garbage may spuriously read as "already
   initialized". Static storage duration satisfies this automatically; for any
   other placement, memset it to zero first.

A complete first program
------------------------

The example below is the minimal lifecycle with real error handling. Each API
call returns a :c:type:`ds_err_code_t`; :c:macro:`ERROR_DS_OK` (zero) means
success. See :doc:`error-codes` for the full list and :doc:`usage` for the
recommended handling pattern.

.. code-block:: c

   #include <stdio.h>
   #include <stdlib.h>
   #include "dynostatic-buffer.h"

   int main(void)
   {
       /* Static storage: zeroed for free (required before first init) and
        * keeps the sizeable arena off the stack. */
       static dynostatic_buffer_t ds_buffer;

       if (ds_initialize_allocation(&ds_buffer) != ERROR_DS_OK) {
           return EXIT_FAILURE;
       }

       /* The pointer MUST be NULL going in: ds_malloc refuses to overwrite a
        * pointer that already refers to a live block. */
       int *numbers = NULL;
       if (ds_malloc(&ds_buffer, (void **)&numbers, 4 * sizeof(int)) != ERROR_DS_OK) {
           ds_deinit_allocation(&ds_buffer);
           return EXIT_FAILURE;
       }

       for (int i = 0; i < 4; ++i) {
           numbers[i] = (i + 1) * 10;
       }
       printf("Allocated 4 ints: %d %d %d %d\n",
              numbers[0], numbers[1], numbers[2], numbers[3]);

       /* On success ds_free sets the pointer back to NULL, so the same
        * variable is immediately safe to reuse. */
       ds_free(&ds_buffer, (void **)&numbers);

       ds_deinit_allocation(&ds_buffer);
       return EXIT_SUCCESS;
   }

Running the bundled examples
----------------------------

The ``examples/`` directory contains four small programs, each focused on one
facet of the API. After a CMake build they land next to the library:

.. code-block:: sh

   ./build/release/basic_example
   ./build/release/array_calloc_realloc
   ./build/release/buffer_introspection
   ./build/release/safe_memory_ops

They are the same programs the :doc:`usage` guide walks through.
