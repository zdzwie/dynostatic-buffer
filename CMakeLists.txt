cmake_minimum_required(VERSION 3.14)

project(Dynostatic_Example C CXX)

option(DS_BUILD_TESTS "Build the unit tests" ON)
option(DS_ZERO_ON_FREE "Zero the contents of freed blocks in dynostatic-buffer" OFF)

add_subdirectory(library)

add_executable(Dynostatic_Example examples/basic_example.c)
target_link_libraries(Dynostatic_Example PUBLIC dynostatic_buffer)

if(DS_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests/unit)
endif()
