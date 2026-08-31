#include "abi.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "PackRequest.h"
#include "PackResponse.h"
#include "Packer.h"
#include "nlohmann/json.hpp"

namespace {

char* CopyToCString(const std::string& text) {
  char* buffer = static_cast<char*>(std::malloc(text.size() + 1));
  if (buffer == nullptr) return nullptr;
  std::memcpy(buffer, text.c_str(), text.size() + 1);
  return buffer;
}

}  // namespace

extern "C" char* fitsolver_solve(const char* request_json) {
  try {
    static fitsolver::CachingPacker packer;  // created once, reused for every call
    const fitsolver::PackRequest request =
        fitsolver::PackRequest::FromJsonString(request_json != nullptr ? request_json : "{}");
    const fitsolver::PackResponse response = fitsolver::Packer().Pack(request);
    return CopyToCString(response.Dump(-1));
  } catch (const std::exception& e) {
    nlohmann::json error;
    error["success"] = false;
    error["error"] = e.what();
    return CopyToCString(error.dump());
  }
}

extern "C" void fitsolver_free(void* pointer) { std::free(pointer); }
