dynostatic-buffer
=================

Implementation of dynamic memory allocation in a predefined static buffer.

``dynostatic-buffer`` provides ``malloc``/``calloc``/``realloc``/``free``-style
allocation backed by a fixed, statically declared buffer, which makes it
suitable for embedded targets where heap allocation is unavailable or
undesirable.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   api

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
