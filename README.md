# dynostatic-buffer

[![CI](https://github.com/zdzwie/dynostatic-buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/zdzwie/dynostatic-buffer/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/zdzwie/dynostatic-buffer/branch/master/graph/badge.svg)](https://codecov.io/gh/zdzwie/dynostatic-buffer)
[![docs](https://img.shields.io/badge/docs-online-blue)](https://zdzwie.github.io/dynostatic-buffer/)

Implementation of dynamic memory allocation in predefined static buffer.

## Building

The library can be built with three build systems. CMake is the primary one and
also drives the unit tests, coverage and documentation; Bazel and a plain
Makefile are provided for convenience and for embedding into other projects.

### CMake

```sh
cmake --preset release
cmake --build --preset release
```

### Bazel

The Bazel build is pinned to Bazel 8 via `.bazelversion` (bazelisk picks it up
automatically) and pulls GoogleTest from the Bazel Central Registry:

```sh
bazel build //...          # library + examples
bazel test //...           # also run the unit tests
bazel run //examples:basic_example
```

### Make

The top-level `Makefile` builds the library and examples directly:

```sh
make            # static + shared library into build/make/
make examples   # build every example program into build/make/
make DS_ZERO_ON_FREE=1   # override options on the command line
```

#### Embedding into another project

The library's build settings live in a relocatable, includable Makefile
fragment (`library/dynostatic-buffer.mk`), so another project can vendor
dynostatic-buffer (submodule, subtree, copy) and reuse it without duplicating
compile flags. Add one line to your Makefile and use the exported variables:

```make
include third_party/dynostatic-buffer/library/dynostatic-buffer.mk

# Compile your own code that includes the header with these flags: the DS_*
# defines change sizeof(dynostatic_buffer_t), so every consumer must match.
app.o: app.c
	$(CC) $(DYNOSTATIC_BUFFER_CPPFLAGS) -c $< -o $@

# Link against the archive the fragment knows how to build.
app: app.o $(DYNOSTATIC_BUFFER_A)
	$(CC) $^ -o $@
```

Everything is namespaced with the `DYNOSTATIC_BUFFER_` / `DS_` prefixes to avoid
clashing with the host project. Useful knobs (all overridable):

| Variable | Purpose |
|----------|---------|
| `DYNOSTATIC_BUFFER_CPPFLAGS` | include path + layout defines for consumer compiles |
| `DYNOSTATIC_BUFFER_A` | path to the built static library |
| `DYNOSTATIC_BUFFER_BUILD_DIR` | where artefacts are written (default `build/dynostatic-buffer`) |
| `DS_ZERO_ON_FREE` | set to `1` to zero freed blocks (library-private) |
| `DS_BUFFER_MEMORY_SIZE`, `DS_MAX_ALLOCATION_COUNT`, ... | buffer layout configuration |

## Examples

The [`examples/`](examples/) directory holds small, self-contained programs
that each demonstrate one facet of the API. They build with all three build
systems above (each is a standalone `*.c` file, so new examples are picked up
automatically):

| Example | Shows |
|---------|-------|
| `basic_example` | initialize, `ds_malloc`, use, `ds_free`, deinit |
| `array_calloc_realloc` | zero-filled arrays with `ds_calloc` and growing with `ds_realloc` |
| `buffer_introspection` | the read-only query API (usage, free slots, largest allocation) |
| `safe_memory_ops` | bounds-checked writes via `ds_safe_memory_set`/`ds_safe_memory_copy` |

Run one after building, e.g. with CMake:

```sh
./build/release/basic_example
```

See [`examples/README.md`](examples/README.md) for a per-example description.

## Documentation

The full API documentation is built with [Sphinx](https://www.sphinx-doc.org)
(Read the Docs theme) and published to GitHub Pages on every push to `master`:

**https://zdzwie.github.io/dynostatic-buffer/**

To build it locally:

```sh
pip install -r docs/requirements.txt   # requires doxygen to be installed too
cmake --preset docs
cmake --build --preset docs
# Output: build/docs/sphinx/index.html
```

See [`docs/README.md`](docs/README.md) for details.
