cmake_minimum_required(VERSION 3.14)
project(my_project)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)


# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable( ds_tests
                utests-common.cpp
                utests-init.cpp
                utests-malloc.cpp
                utests-free.cpp
                ../../library/dynostatic-buffer.c
)

target_include_directories(ds_tests PUBLIC
      "${PROJECT_SOURCE_DIR}/../../library"
      "${PROJECT_SOURCE_DIR}/../utils"
    )
add_definitions(-DTEST_LIBRARY)

target_link_libraries(
  ds_tests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ds_tests)
