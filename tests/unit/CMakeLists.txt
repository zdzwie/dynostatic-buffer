set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable( ds_tests
                utests-common.cpp
                utests-init.cpp
                utests-malloc.cpp
                utests-free.cpp
                utests-multi-alloc.cpp
                utests-alligment.cpp
                utests-calloc.cpp
                utests-realloc.cpp
                utests-memory-usage.cpp
                utests-getters.cpp
        utests-safe-memory-copy.cpp
        utests-safe-memory-set.cpp
)

target_link_libraries(ds_tests PRIVATE
  dynostatic_buffer
  GTest::gtest_main
)

target_include_directories(ds_tests PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../utils"
    )

include(GoogleTest)
gtest_discover_tests(ds_tests)
