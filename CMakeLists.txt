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
set(FITSOLVER_CORE_SOURCES
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
  src/PackingEntropy.cpp
  src/EntropyItemSorter.cpp
  src/EntropyBoxSorter.cpp
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
  src/PackedLayer.cpp
  src/LayerStabiliser.cpp
  src/OrientatedItemFactory.cpp
  src/OrientatedItemSorter.cpp
  src/LayerPacker.cpp
  src/VolumePacker.cpp
  src/WeightRedistributor.cpp
  src/Packer.cpp
  src/PackRequest.cpp
  src/PackResponse.cpp
)

add_library(fitsolver_core ${FITSOLVER_CORE_SOURCES})
target_include_directories(fitsolver_core PUBLIC src)
target_link_libraries(fitsolver_core PUBLIC nlohmann_json)
# fitsolver_core is linked into fitsolver_lib (SHARED) below, so its object
# files must be position-independent -- MSVC ignores this, but GCC/Clang
# refuse to link a non-PIC static lib into a shared one ("recompile with
# -fPIC").
set_target_properties(fitsolver_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Manual timing / benchmark harnesses ---------------------------------
# Kept out of the default build and CTest because benchmark results should
# only be collected intentionally (and preferably from a Release build).
# Their sources live under tests/ and benchmarks/, which the production
# Dockerfile doesn't copy -- and CMake refuses to generate a target whose
# source is missing even when it's EXCLUDE_FROM_ALL -- so the image build
# turns this off (see Dockerfile).
option(FITSOLVER_BUILD_BENCHMARKS "Define the manual timing/benchmark targets" ON)

if(FITSOLVER_BUILD_BENCHMARKS)
  add_executable(fitsolver_timed_test EXCLUDE_FROM_ALL
    tests/manual/timed_test.cpp
  )
  target_link_libraries(fitsolver_timed_test PRIVATE fitsolver_core)

  # Scenario timing + output fingerprint (see tests/manual/scenario_bench.cpp).
  add_executable(fitsolver_scenario_bench EXCLUDE_FROM_ALL
    tests/manual/scenario_bench.cpp
  )
  target_link_libraries(fitsolver_scenario_bench PRIVATE fitsolver_core)
  target_compile_definitions(fitsolver_scenario_bench PRIVATE
    FITSOLVER_THOMMEAX_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ThommeaxMinimumTests")

  # BoxPacker (PHP) comparison harness -- see benchmarks/boxpacker_comparison/.
  add_executable(fitsolver_boxpacker_bench EXCLUDE_FROM_ALL
    benchmarks/boxpacker_comparison/run_fitsolver.cpp
  )
  target_link_libraries(fitsolver_boxpacker_bench PRIVATE fitsolver_core)
endif()

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
    tests/integration/packer_test.cpp
    tests/integration/thommeax_benchmark_test.cpp
    tests/integration/weight_redistribution_test.cpp
    tests/unit/rotation_test.cpp
    tests/unit/orientateditem_test.cpp
    tests/unit/voidfinder_test.cpp
    tests/unit/voidspace_test.cpp
    tests/unit/item_classes_test.cpp
    tests/unit/box_classes_test.cpp
    tests/unit/packed_classes_test.cpp
    tests/unit/request_classes_test.cpp
    tests/unit/packing_classes_test.cpp
    tests/unit/entropy_test.cpp
  )
  target_link_libraries(fitsolver_tests
    PRIVATE
      fitsolver_core
      gtest_main
  )
  # Absolute path so thommeax_benchmark_test.cpp can load its fixture JSON
  # regardless of the test binary's working directory (ctest vs running it
  # directly, different per OS/IDE).
  target_compile_definitions(fitsolver_tests PRIVATE
    FITSOLVER_THOMMEAX_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ThommeaxMinimumTests"
    FITSOLVER_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

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

# --- Coverage-guided fuzzing (Clang/libFuzzer + sanitizers) -------------
# Disabled for ordinary builds. CI enables it in a separate Clang build so
# the complete core is instrumented without adding sanitizer requirements to
# the application, shared library, Python module, or normal GoogleTest run.
option(FITSOLVER_BUILD_FUZZER "Build the PackRequest libFuzzer target" OFF)

if(FITSOLVER_BUILD_FUZZER)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "FITSOLVER_BUILD_FUZZER requires Clang/libFuzzer")
  endif()

  add_library(fitsolver_fuzz_core STATIC ${FITSOLVER_CORE_SOURCES})
  target_include_directories(fitsolver_fuzz_core PUBLIC src)
  target_link_libraries(fitsolver_fuzz_core PUBLIC nlohmann_json)
  target_compile_options(fitsolver_fuzz_core PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
  )

  add_executable(fuzz_pack_request tests/fuzz/fuzz_pack_request.cpp)
  target_link_libraries(fuzz_pack_request PRIVATE fitsolver_fuzz_core)
  target_compile_options(fuzz_pack_request PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
  )
  target_link_options(fuzz_pack_request PRIVATE
    -fsanitize=fuzzer,address,undefined
  )
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
  install(TARGETS fitsolver
    LIBRARY DESTINATION python/fitsolver
    RUNTIME DESTINATION python/fitsolver
  )
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/python/fitsolver/
    DESTINATION fitsolver
    FILES_MATCHING
    PATTERN "*.json"
  )
endif()
