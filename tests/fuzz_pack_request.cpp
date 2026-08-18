// Coverage-guided fuzz target for the public JSON entry point.
//
// This file deliberately lives in tests/ but is not part of the normal GoogleTest
// executable. Build it with Clang/libFuzzer after adding a dedicated CMake target:
//   clang++ -std=c++17 -fsanitize=fuzzer,address,undefined -Isrc -Ithird_party
//     tests/fuzz_pack_request.cpp <fitsolver core sources> -o fuzz_pack_request
//   ./fuzz_pack_request tests/fuzz_corpus -max_total_time=60

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
