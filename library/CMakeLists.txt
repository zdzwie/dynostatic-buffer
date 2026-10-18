# Declared here rather than in the top-level project, so a consumer that pulls
# in only this directory (add_subdirectory(library)) gets the same default.
option(DS_ZERO_ON_FREE "Zero the contents of freed blocks in dynostatic-buffer" ON)
set(DS_ALIGNMENT "" CACHE STRING
    "Alignment of returned pointers (power of two); empty uses the header default alignof(ds_max_align_t)")

add_library(dynostatic_buffer dynostatic-buffer.c)
include(../scripts/cmake/generate_doc.cmake)

# The public header relies on C11 (_Static_assert, <stdalign.h>'s alignof/
# alignas, max_align_t). GCC/Clang default to gnu11+, but MSVC defaults to a
# pre-C11 dialect and rejects them, so require C11 explicitly. PUBLIC so every
# consumer (example, unit tests) compiles the header in the same mode.
target_compile_features(dynostatic_buffer PUBLIC c_std_11)

# Expose the public header location to anyone linking the library
# (examples and unit tests), so they include the same header.
target_include_directories(dynostatic_buffer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# These defines change the layout of dynostatic_buffer_t (array sizes), so they
# MUST be visible to every consumer of the header, not just the .c translation
# unit. Keeping them PUBLIC guarantees the library and its consumers agree on
# sizeof(dynostatic_buffer_t).
target_compile_definitions(dynostatic_buffer PUBLIC
                           DS_BUFFER_MEMORY_SIZE=1024
                           DS_LOG_ENABLE=1u
                           DS_MAX_ALLOCATION_COUNT=10
                           DS_MAX_ALLOCATION_SIZE=512
                           )

# DS_ALIGNMENT changes the layout too (alignas on the arena, block rounding),
# so an explicit value is PUBLIC like the defines above. Left empty, nothing is
# defined and every translation unit falls back to the same header default.
if(NOT DS_ALIGNMENT STREQUAL "")
    target_compile_definitions(dynostatic_buffer PUBLIC DS_ALIGNMENT=${DS_ALIGNMENT})
endif()

# DS_ZERO_ON_FREE only changes ds_free()'s runtime behaviour, not the layout of
# dynostatic_buffer_t, but it is still PUBLIC: consumers (and the unit tests,
# which assert that delivered blocks arrive clean) must see the same policy the
# library was built with. $<BOOL:...> expands to 1/0.
target_compile_definitions(dynostatic_buffer PUBLIC
                           DS_ZERO_ON_FREE=$<BOOL:${DS_ZERO_ON_FREE}>
                           )
