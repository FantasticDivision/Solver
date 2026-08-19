// Minimal standalone usage sample for the C++ core -- not part of the
// test suite (see ../integration/packer_test.cpp for that), just a quick way to see the
// ItemList/BoxList/Packer API in isolation. Build it yourself, e.g.:
//   g++ -std=c++17 -Isrc -Ithird_party tests/manual/test_solver.cpp src/*.cpp -o test_solver
#include <memory>

#include "BoxList.h"
#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "ItemList.h"
#include "PackResponse.h"
#include "Packer.h"

#include <iostream>

int main() {
  fitsolver::ItemList items;
  items.Insert(std::make_shared<fitsolver::ConcreteItem>("ITM-001", "Widget A", 100, 200, 50, 1.0));

  fitsolver::BoxList boxes;
  boxes.Insert(std::make_shared<fitsolver::ConcreteBox>("SML", 300, 300, 300));

  fitsolver::PackResponse response = fitsolver::Packer().Pack(items, boxes);

  std::cout << response.Dump() << std::endl;
  return 0;
}
