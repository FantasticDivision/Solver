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
#include <sstream>

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
    const fitsolver::PackResponse response = fitsolver::Packer().Pack(request);
    std::cout << response.Dump(2) << std::endl;
    return response.success ? 0 : 2;
  } catch (const std::exception& e) {
    std::cerr << "fitsolver_app: " << e.what() << "\n";
    return 1;
  }
}
