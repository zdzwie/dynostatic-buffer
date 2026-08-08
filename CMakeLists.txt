cmake_minimum_required(VERSION 3.14)

project(Dynostatic_Example C CXX)

option(DS_BUILD_TESTS "Build the unit tests" ON)
option(DS_ZERO_ON_FREE "Zero the contents of freed blocks in dynostatic-buffer" OFF)

add_subdirectory(library)

file(GLOB DS_EXAMPLE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c")
foreach(example_source ${DS_EXAMPLE_SOURCES})
    get_filename_component(example_name "${example_source}" NAME_WE)
    add_executable(${example_name} "${example_source}")
    target_link_libraries(${example_name} PUBLIC dynostatic_buffer)
endforeach()

if(DS_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests/unit)
endif()
