cmake_minimum_required(VERSION 3.16)
project(FitSolver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Main library/executable -------------------------------------------
# Swap this for your real source files as the project grows.
add_library(fitsolver_core src/packer.cpp)
target_include_directories(fitsolver_core PUBLIC src)

add_executable(fitsolver_app src/main.cpp)
target_link_libraries(fitsolver_app PRIVATE fitsolver_core)

# --- Tests (Google Test via FetchContent) -------------------------------
# FetchContent pulls GoogleTest at configure time so contributors don't
# need to install it locally, and CI always builds against a pinned version.
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
# Prevent GoogleTest from overriding our compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(fitsolver_tests tests/packer_test.cpp)
target_link_libraries(fitsolver_tests
  PRIVATE
    fitsolver_core
    gtest_main
)

# Registers the test binary with CTest, which is what `ctest` in the
# CI workflow actually runs.
include(GoogleTest)
gtest_discover_tests(fitsolver_tests)
