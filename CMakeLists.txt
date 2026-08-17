cmake_minimum_required(VERSION 3.18)
project(FitSolver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /EHsc)
else()
  add_compile_options(-Wall -Wextra)
endif()

# Needed by both the (optional) GoogleTest fetch below and the
# (optional) pybind11 fetch further down -- included once, up front, so
# neither option block depends on the other having run first.
include(FetchContent)

# --- nlohmann::json (vendored, header-only) ------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# --- Core packing engine --------------------------------------------------
# Everything under src/ except the three "front door" entry points below
# (main.cpp, abi.cpp, pymodule.cpp) — those each link against this once.
# One file per class (Item/LinkedItem, Box/LimitedSupplyBox, the List and
# Sorter types, the packing engine itself, ...), mirroring the structure
# of the DVDoug/BoxPacker PHP library this design is inspired by.
add_library(fitsolver_core
  src/Rotation.cpp
  src/Item.cpp
  src/LinkedItem.cpp
  src/ConcreteItem.cpp
  src/Box.cpp
  src/LimitedSupplyBox.cpp
  src/ConcreteBox.cpp
  src/DefaultItemSorter.cpp
  src/DefaultBoxSorter.cpp
  src/DefaultPackedBoxSorter.cpp
  src/ItemList.cpp
  src/BoxList.cpp
  src/OrientatedItem.cpp
  src/PackedItem.cpp
  src/PackedItemList.cpp
  src/PackedBox.cpp
  src/PackedBoxList.cpp
  src/VoidSpace.cpp
  src/VoidFinder.cpp
  src/WorkingVolume.cpp
  src/VolumePacker.cpp
  src/WeightRedistributor.cpp
  src/Packer.cpp
  src/PackRequest.cpp
  src/PackResponse.cpp
)
target_include_directories(fitsolver_core PUBLIC src)
target_link_libraries(fitsolver_core PUBLIC nlohmann_json)
# fitsolver_core is linked into fitsolver_lib (SHARED) below, so its object
# files must be position-independent -- MSVC ignores this, but GCC/Clang
# refuse to link a non-PIC static lib into a shared one ("recompile with
# -fPIC").
set_target_properties(fitsolver_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- CLI executable (reads a JSON request from stdin, prints the response)
add_executable(fitsolver_app src/main.cpp)
target_link_libraries(fitsolver_app PRIVATE fitsolver_core)

# --- Shared library exposing a plain C ABI (ctypes-friendly) -------------
# Alternative to the Python extension module below for non-Python callers,
# or for calling from Python without building a CPython extension at all
# (see example/python_example.py).
add_library(fitsolver_lib SHARED src/abi.cpp)
target_link_libraries(fitsolver_lib PRIVATE fitsolver_core)
set_target_properties(fitsolver_lib PROPERTIES
  OUTPUT_NAME "fitsolver_lib"
  PREFIX ""
)

# --- Tests (Google Test via FetchContent) -------------------------------
# On by default; turn it off with -DFITSOLVER_BUILD_TESTS=OFF so a
# production image build (see Dockerfile) doesn't need network access to
# fetch a test framework it will never run, and doesn't spend time
# building a test binary that never ships.
option(FITSOLVER_BUILD_TESTS "Build the fitsolver_tests Google Test suite" ON)

if(FITSOLVER_BUILD_TESTS)
  # FetchContent pulls GoogleTest at configure time so contributors don't
  # need to install it locally, and CI always builds against a pinned version.
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
  )
  # Prevent GoogleTest from overriding our compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  add_executable(fitsolver_tests 
  tests/packer_test.cpp
  tests/rotation_test.cpp
  tests/orientateditem_test.cpp)
  target_link_libraries(fitsolver_tests
    PRIVATE
      fitsolver_core
      gtest_main
  )

  # Registers the test binary with CTest, which is what `ctest` in the
  # CI workflow actually runs. A plain add_test() (rather than
  # gtest_discover_tests(), which runs the freshly-built binary as a build
  # step just to enumerate individual test names) keeps the build itself
  # from depending on being able to execute binaries in restricted/
  # sandboxed build environments. `ctest` sees one test here that runs the
  # whole suite; run the binary directly (or `ctest -V`) to see each
  # individual TEST() case's own pass/fail.
  add_test(NAME fitsolver_tests COMMAND fitsolver_tests)
endif()

# --- Python extension module (pybind11) -----------------------------------
# Builds the real `fitsolver` module that `import fitsolver` picks up.
# On by default; turn it off with -DFITSOLVER_BUILD_PYTHON=OFF in
# environments with no Python development headers available (e.g. the
# slim production Docker image, which only ships the fitsolver_app CLI).
option(FITSOLVER_BUILD_PYTHON "Build the fitsolver Python extension module" ON)

if(FITSOLVER_BUILD_PYTHON)
  set(PYBIND11_FINDPYTHON ON)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v3.0.4
  )
  FetchContent_MakeAvailable(pybind11)

  pybind11_add_module(fitsolver src/pymodule.cpp)
  target_link_libraries(fitsolver PRIVATE fitsolver_core)
endif()
