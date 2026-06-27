add_library(dynostatic_buffer dynostatic-buffer.c)
include(../scripts/cmake/generate_doc.cmake)

# Expose the public header location to anyone linking the library
# (examples and unit tests), so they include the same header.
target_include_directories(dynostatic_buffer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# These defines change the layout of dynostatic_buffer_t (array sizes), so they
# MUST be visible to every consumer of the header, not just the .c translation
# unit. Keeping them PUBLIC guarantees the library and its consumers agree on
# sizeof(dynostatic_buffer_t).
target_compile_definitions(dynostatic_buffer PUBLIC
                           DS_BUFFER_MEMORY_SIZE=1024
                           DS_LOG_ENABLE=true
                           DS_MAX_ALLOCATION_COUNT=10
                           DS_MAX_ALLOCATION_SIZE=512
                           )
