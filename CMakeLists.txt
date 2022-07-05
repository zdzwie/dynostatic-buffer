cmake_minimum_required(VERSION 3.10)

project(Dynostatic_Example)
add_subdirectory(library)

add_executable(Dynostatic_Example examples/basic_example.c)
target_link_libraries(Dynostatic_Example PUBLIC dynostatic_buffer)

target_include_directories(Dynostatic_Example PUBLIC
                        "${PROJECT_BINARY_DIR}"
                        "${PROJECT_SOURCE_DIR}/library")
