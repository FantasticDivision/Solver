// fitsolver_app -- minimal CLI wrapper around the FitSolver core.
//
// Reads a JSON packing request ({"items": [...], "boxes": [...]}) from
// stdin (or from a file given as argv[1]) and writes the JSON response to
// stdout. Useful for quick manual checks and for shelling out from other
// languages/services without linking against the library directly.
//
//   fitsolver_app < request.json
//   fitsolver_app request.json
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>

#include "SolutionCache.h" 
#include "PackRequest.h"
#include "PackResponse.h"
#include "Packer.h"

namespace {

std::string ReadAll(std::istream& in) {
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

}  // namespace

int main(int argc, char** argv) {
  std::string request_text;

  if (argc > 1) {
    std::ifstream file(argv[1]);
    if (!file) {
      std::cerr << "fitsolver_app: could not open '" << argv[1] << "'\n";
      return 1;
    }
    request_text = ReadAll(file);
  } else {
    request_text = ReadAll(std::cin);
  }

  try {
        const fitsolver::PackRequest request = fitsolver::PackRequest::FromJsonString(request_text);
    
    // 1. Create the cache and packer
    static fitsolver::SolutionCache cache(256); 
    const fitsolver::Packer packer;

    // 2. First Call (Cold Cache)
    auto start1 = std::chrono::high_resolution_clock::now();
    const fitsolver::PackResponse response = fitsolver::PackCached(
        packer, request.GetItems(), request.GetBoxes(), cache);
    auto end1 = std::chrono::high_resolution_clock::now();
    std::cerr << "Cold Cache Time: " 
              << std::chrono::duration<double, std::milli>(end1 - start1).count() 
              << " ms | Cache Size: " << cache.Count() << "\n";

    // 3. Second Call (Warm Cache - exact same request)
    auto start2 = std::chrono::high_resolution_clock::now();
    const fitsolver::PackResponse response2 = fitsolver::PackCached(
        packer, request.GetItems(), request.GetBoxes(), cache);
    auto end2 = std::chrono::high_resolution_clock::now();
    std::cerr << "Warm Cache Time: " 
              << std::chrono::duration<double, std::milli>(end2 - start2).count() 
              << " ms | Cache Size: " << cache.Count() << "\n";

    std::cout << response.Dump(2) << std::endl;
    return response.success ? 0 : 2;
  } catch (const std::exception& e) {
    std::cerr << "fitsolver_app: " << e.what() << "\n";
    return 1;
  }
} 
