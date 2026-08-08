# Examples

Small, self-contained programs demonstrating the dynostatic-buffer API. Each
example is a single `*.c` file that includes the shared header-only helper
[`example_common.h`](example_common.h) (error-code stringification and a
`DS_EXAMPLE_CHECK` macro). Because they are plain source files, all three build
systems discover them automatically — just drop a new `.c` file here.

| File | Demonstrates |
|------|--------------|
| [`basic_example.c`](basic_example.c) | The minimal lifecycle: `ds_initialize_allocation` → `ds_malloc` → use → `ds_free` → `ds_deinit_allocation`. |
| [`array_calloc_realloc.c`](array_calloc_realloc.c) | Zero-initialized arrays with `ds_calloc`, then growing them in place with `ds_realloc` while preserving contents. |
| [`buffer_introspection.c`](buffer_introspection.c) | The read-only query API: `ds_get_memory_usage`, `ds_get_free_allocator_cnt`, `ds_get_max_new_allocation_size`. |
| [`safe_memory_ops.c`](safe_memory_ops.c) | Bounds-checked writes with `ds_safe_memory_set` / `ds_safe_memory_copy`, including a rejected over-long copy. |

## Building and running

All commands are run from the repository root.

### CMake

```sh
cmake --preset release
cmake --build --preset release
./build/release/basic_example
```

### Bazel

```sh
bazel run //examples:basic_example
```

### Make

```sh
make examples          # builds every example into build/make/
./build/make/basic_example
```

The Make and CMake builds compile examples with the same layout-affecting
`DS_*` defines as the library, exactly as an external consumer must (see the
"Embedding into another project" section of the top-level
[README](../README.md)).
