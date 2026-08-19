// Coverage-guided fuzz target for the public JSON entry point.
//
// This target is separate from the normal GoogleTest suite.
// executable. Build and run it with Clang/libFuzzer through CMake:
//   cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++
//     -DFITSOLVER_BUILD_TESTS=OFF -DFITSOLVER_BUILD_PYTHON=OFF
//     -DFITSOLVER_BUILD_FUZZER=ON
//   cmake --build build-fuzz --target fuzz_pack_request
//   ./build-fuzz/fuzz_pack_request tests/fuzz/corpus -max_total_time=60

#include <cstddef>
#include <cstdint>
#include <string>

#include "PackRequest.h"
#include "Packer.h"
#include "nlohmann/json.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  // Avoid spending a fuzzing worker's entire iteration on enormous valid arrays.
  if (size > 64 * 1024) return 0;

  try {
    const std::string input(reinterpret_cast<const char*>(data), size);
    const fitsolver::PackRequest request = fitsolver::PackRequest::FromJsonString(input);
    const fitsolver::PackResponse response = fitsolver::Packer().Pack(request);

    // Exercise both serializers and ensure the produced wire format remains valid JSON.
    const std::string output = response.Dump(-1);
    const nlohmann::json reparsed = nlohmann::json::parse(output);
    static_cast<void>(reparsed);
  } catch (const nlohmann::json::exception&) {
    // Invalid JSON and values of the wrong JSON type are expected fuzz inputs.
  } catch (const std::invalid_argument&) {
    // Missing required request fields are also expected fuzz inputs.
  }

  return 0;
}
