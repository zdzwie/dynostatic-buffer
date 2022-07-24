add_library(dynostatic_buffer dynostatic-buffer.c)
include(../scripts/cmake/generate_doc.cmake)

add_definitions(-DDS_BUFFER_MEMORY_SIZE=1024
                -DDS_LOG_ENABLE=true
                -DDS_MAX_ALLOCATION_COUNT=10
                -DDS_MAX_ALLOCATION_SIZE=512
                )