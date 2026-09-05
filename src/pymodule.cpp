// Python bindings for the FitSolver core, built as a native CPython
// extension module named `fitsolver` (via pybind11).
//
// Three ways to call it, from simplest to richest:
//
//   1. fitsolver.solve(json_str) -> json_str
//        Pure JSON in, JSON out. No fitsolver types required at all.
//
//   2. fitsolver.pack_dicts(items: list[dict], boxes: list[dict]) -> PackResult
//        Pass plain Python dicts shaped like items.json / boxes.json,
//        get back a rich PackResult object with attribute access to
//        every computed feature (box weight, item orientation, volume
//        utilisation, remaining space, ...).
//
//   3. fitsolver.pack(items: list[Item], boxes: list[Box]) -> PackResult
//        Same rich result, but built from fitsolver.Item / fitsolver.Box
//        objects instead of dicts.
//
// The underlying C++ model (Item/LinkedItem, Box/LimitedSupplyBox, ...)
// is interface-based -- see src/Item.h, src/Box.h and friends. From
// Python this is invisible: fitsolver.Item and fitsolver.Box always
// look the same regardless of which concrete C++ subclass a given
// instance actually is (plain vs. linked / quantity-limited); a
// dynamic_cast under the hood is all that changes.
//
// All three entry points share one CachingPacker (see SharedPacker()
// below), so solving the same request via different entry points still
// hits the cache.
//
// See example/*.py for runnable demonstrations of all three.
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <optional>
#include <string>
#include <vector>

#include "CachingPacker.h"
#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "LimitedSupplyBox.h"
#include "LinkedItem.h"
#include "PackRequest.h"
#include "PackResponse.h"

namespace py = pybind11;
using namespace fitsolver;

namespace {

// One CachingPacker, created once and shared by solve()/pack_dicts()/
// pack() below -- `static` local means it's constructed on first use
// and then persists for the lifetime of the process, so the cache
// actually accumulates hits across calls instead of resetting each time.
CachingPacker& SharedPacker() {
  static CachingPacker packer;
  return packer;
}

// Recursively converts an nlohmann::json value into native Python
// objects (dict/list/str/float/int/bool/None), so every *.to_dict()
// binding below can just be `JsonToPy(self.ToJson())`.
py::object JsonToPy(const nlohmann::json& j) {
  if (j.is_null()) return py::none();
  if (j.is_boolean()) return py::bool_(j.get<bool>());
  if (j.is_number_integer() || j.is_number_unsigned()) return py::int_(j.get<long long>());
  if (j.is_number_float()) return py::float_(j.get<double>());
  if (j.is_string()) return py::str(j.get<std::string>());
  if (j.is_array()) {
    py::list list;
    for (const auto& element : j) list.append(JsonToPy(element));
    return list;
  }
  if (j.is_object()) {
    py::dict dict;
    for (auto it = j.begin(); it != j.end(); ++it) dict[py::str(it.key())] = JsonToPy(it.value());
    return dict;
  }
  return py::none();
}

// Converts a Python list of dicts (already validated to be JSON-able) to
// an nlohmann::json array by round-tripping through the stdlib `json`
// module -- far less bespoke code than hand-walking arbitrary py::object
// trees, and it gets every edge case (numpy scalars aside) right for
// free.
nlohmann::json PyListToJson(const py::object& value) {
  py::object json_module = py::module_::import("json");
  const std::string text = json_module.attr("dumps")(value).cast<std::string>();
  return nlohmann::json::parse(text);
}

// Factory backing fitsolver.Item(...) from Python -- decides whether a
// plain ConcreteItem or a ConcreteLinkedItem (when box_group is given)
// is what actually gets constructed, exactly like MakeItemFromJson does
// for the JSON wire format.
ItemPtr MakeItemFromArgs(std::string item_code, std::string item_reference, double width,
                          double length, double depth, double weight, std::string box_group,
                          Rotation allowed_rotation) {
  ConcreteItem base(std::move(item_code), std::move(item_reference), width, length, depth, weight,
                     allowed_rotation);
  if (box_group.empty()) return std::make_shared<ConcreteItem>(std::move(base));
  return std::make_shared<ConcreteLinkedItem>(std::move(base), std::move(box_group));
}

// Factory backing fitsolver.Box(...) from Python -- decides whether a
// plain ConcreteBox or a ConcreteLimitedSupplyBox (when max_quantity is
// given) is what actually gets constructed.
BoxPtr MakeBoxFromArgs(std::string reference, double width, double length, double depth,
                        double max_weight, double empty_weight, bool active,
                        std::optional<int> max_quantity) {
  ConcreteBox base(std::move(reference), width, length, depth, max_weight, empty_weight, active);
  if (max_quantity.has_value()) {
    return std::make_shared<ConcreteLimitedSupplyBox>(std::move(base), *max_quantity);
  }
  return std::make_shared<ConcreteBox>(std::move(base));
}

std::vector<PackedItem> PackedItemsOf(const PackedBox& box) { return box.GetItems().ToVector(); }
std::vector<PackedBox> BoxesOf(const PackResponse& response) {
  return {response.boxes.begin(), response.boxes.end()};
}
std::vector<ItemPtr> UnplacedItemsOf(const PackResponse& response) {
  return {response.unplaced_items.begin(), response.unplaced_items.end()};
}

PackResponse PackDicts(const py::object& item_dicts, const py::object& box_dicts) {
  ItemList items;
  for (const auto& item_json : PyListToJson(item_dicts)) items.Insert(MakeItemFromJson(item_json));
  BoxList boxes;
  for (const auto& box_json : PyListToJson(box_dicts)) boxes.Insert(MakeBoxFromJson(box_json));
  return SharedPacker().Pack(items, boxes);
}

PackResponse PackObjects(const std::vector<ItemPtr>& items, const std::vector<BoxPtr>& boxes) {
  ItemList item_list;
  for (const auto& item : items) item_list.Insert(item);
  BoxList box_list;
  for (const auto& box : boxes) box_list.Insert(box);
  return SharedPacker().Pack(item_list, box_list);
}

std::string SolveJson(const std::string& request_json) {
  return SharedPacker().Pack(PackRequest::FromJsonString(request_json)).Dump(-1);
}

}  // namespace

PYBIND11_MODULE(fitsolver, m) {
  m.doc() =
      "FantasticSolver 3D bin-packing engine.\n\n"
      "solve(json) -> json is the simplest entry point; pack_dicts(items, boxes) "
      "and pack(items, boxes) return a richer PackResult object.";

  py::enum_<Rotation>(m, "Rotation", "How free an item is to be reoriented while packing.")
      .value("NEVER", Rotation::kNever, "Must be packed exactly as given (Width/Length/Depth).")
      .value("KEEP_FLAT", Rotation::kKeepFlat, "May swap Width/Length but stay 'this way up'.")
      .value("BEST_FIT", Rotation::kBestFit, "May be placed in any of the 6 axis orderings.");

  py::class_<Item, ItemPtr>(m, "Item", "A single item to be packed.")
      .def(py::init(&MakeItemFromArgs), py::arg("item_code"), py::arg("item_reference"),
           py::arg("width"), py::arg("length"), py::arg("depth"), py::arg("weight"),
           py::arg("box_group") = "", py::arg("allowed_rotation") = Rotation::kBestFit)
      .def_property_readonly("item_code", &Item::GetItemCode)
      .def_property_readonly("item_reference", &Item::GetItemReference)
      .def_property_readonly("width", &Item::GetWidth)
      .def_property_readonly("length", &Item::GetLength)
      .def_property_readonly("depth", &Item::GetDepth)
      .def_property_readonly("weight", &Item::GetWeight)
      .def_property_readonly("box_group",
                              [](const Item& self) -> py::object {
                                if (const auto* linked = dynamic_cast<const LinkedItem*>(&self)) {
                                  return py::str(linked->GetBoxGroup());
                                }
                                return py::none();
                              })
      .def_property_readonly("has_box_group",
                              [](const Item& self) { return dynamic_cast<const LinkedItem*>(&self) != nullptr; })
      .def_property_readonly("allowed_rotation", &Item::GetAllowedRotation)
      .def_property_readonly("volume", &Item::GetVolume)
      .def("to_dict", [](const Item& self) { return JsonToPy(self.ToJson()); },
           "Every feature of this item as a plain Python dict.")
      .def("__repr__", [](const Item& self) {
        return "<fitsolver.Item '" + self.GetItemCode() + "'>";
      });

  py::class_<Box, BoxPtr>(m, "Box", "A box type available to pack items into.")
      .def(py::init(&MakeBoxFromArgs), py::arg("reference"), py::arg("width"), py::arg("length"),
           py::arg("depth"), py::arg("max_weight") = kNoWeightLimit, py::arg("empty_weight") = 0.0,
           py::arg("active") = true, py::arg("max_quantity") = std::nullopt)
      .def_property_readonly("reference", &Box::GetReference)
      .def_property_readonly("width", &Box::GetWidth)
      .def_property_readonly("length", &Box::GetLength)
      .def_property_readonly("depth", &Box::GetDepth)
      .def_property_readonly("max_weight", &Box::GetMaxWeight)
      .def_property_readonly("has_weight_limit", &Box::HasWeightLimit)
      .def_property_readonly("empty_weight", &Box::GetEmptyWeight)
      .def_property_readonly("active", &Box::GetActive)
      .def_property_readonly("max_quantity",
                              [](const Box& self) -> py::object {
                                if (const auto* limited = dynamic_cast<const LimitedSupplyBox*>(&self)) {
                                  return py::int_(limited->GetQuantityAvailable());
                                }
                                return py::none();
                              })
      .def_property_readonly("has_limited_quantity",
                              [](const Box& self) { return dynamic_cast<const LimitedSupplyBox*>(&self) != nullptr; })
      .def_property_readonly("inner_volume", &Box::GetInnerVolume)
      .def("to_dict", [](const Box& self) { return JsonToPy(self.ToJson()); },
           "Every feature of this box type as a plain Python dict.")
      .def("__repr__", [](const Box& self) {
        return "<fitsolver.Box '" + self.GetReference() + "'>";
      });

  py::class_<PackedItem>(m, "PackedItem", "An Item placed at a specific position inside a box.")
      .def_property_readonly("item", &PackedItem::GetItem, "The original Item that was placed.")
      .def_property_readonly("x", &PackedItem::GetX)
      .def_property_readonly("y", &PackedItem::GetY)
      .def_property_readonly("z", &PackedItem::GetZ)
      .def_property_readonly("width", &PackedItem::GetWidth, "Placed (post-rotation) width.")
      .def_property_readonly("length", &PackedItem::GetLength, "Placed (post-rotation) length.")
      .def_property_readonly("depth", &PackedItem::GetDepth, "Placed (post-rotation) depth.")
      .def_property_readonly("weight", &PackedItem::GetWeight)
      .def_property_readonly("volume", &PackedItem::GetVolume)
      .def_property_readonly("orientation", &PackedItem::GetOrientationCode,
                              "Axis mapping used, e.g. 'WLD' = original order, "
                              "'LWD' = width/length swapped, etc.")
      .def_property_readonly("rotated", &PackedItem::IsRotated,
                              "True if this item was not placed in its original orientation.")
      .def("to_dict", [](const PackedItem& self) { return JsonToPy(self.ToJson()); })
      .def("__repr__", [](const PackedItem& self) {
        return "<fitsolver.PackedItem '" + self.GetItem()->GetItemCode() + "' @ (" +
               std::to_string(self.GetX()) + ", " + std::to_string(self.GetY()) + ", " +
               std::to_string(self.GetZ()) + ")>";
      });

  py::class_<PackedBox>(m, "PackedBox", "A box together with everything packed into it.")
      .def_property_readonly("box", &PackedBox::GetBox)
      .def_property_readonly("items", &PackedItemsOf)
      .def_property_readonly("weight", &PackedBox::GetWeight, "Empty weight + all item weights.")
      .def_property_readonly("item_weight", &PackedBox::GetItemWeight)
      .def_property_readonly("max_weight", &PackedBox::GetMaxWeight)
      .def_property_readonly("remaining_weight", &PackedBox::GetRemainingWeight)
      .def_property_readonly("used_volume", &PackedBox::GetUsedVolume)
      .def_property_readonly("inner_volume", &PackedBox::GetInnerVolume)
      .def_property_readonly("unused_volume", &PackedBox::GetUnusedVolume)
      .def_property_readonly("volume_utilisation", &PackedBox::GetVolumeUtilisation,
                              "Used volume as a percentage (0-100) of inner volume.")
      .def_property_readonly("used_width", &PackedBox::GetUsedWidth)
      .def_property_readonly("used_length", &PackedBox::GetUsedLength)
      .def_property_readonly("used_depth", &PackedBox::GetUsedDepth)
      .def_property_readonly("remaining_width", &PackedBox::GetRemainingWidth)
      .def_property_readonly("remaining_length", &PackedBox::GetRemainingLength)
      .def_property_readonly("remaining_depth", &PackedBox::GetRemainingDepth)
      .def("to_dict", [](const PackedBox& self) { return JsonToPy(self.ToJson()); })
      .def("__len__", [](const PackedBox& self) { return self.GetItems().Count(); })
      .def("__repr__", [](const PackedBox& self) {
        return "<fitsolver.PackedBox '" + self.GetBox()->GetReference() + "' items=" +
               std::to_string(self.GetItems().Count()) + ">";
      });

  py::class_<PackResponse>(m, "PackResult", "The outcome of a packing run.")
      .def_readonly("success", &PackResponse::success,
                     "True if every item was placed in some box.")
      .def_property_readonly("boxes", &BoxesOf)
      .def_property_readonly("unplaced_items", &UnplacedItemsOf)
      .def_property_readonly("overall_volume_utilisation", &PackResponse::GetOverallVolumeUtilisation)
      .def_property_readonly("total_placed", &PackResponse::GetTotalPlacedCount)
      .def_property_readonly("total_unplaced", &PackResponse::GetTotalUnplacedCount)
      .def("to_dict", [](const PackResponse& self) { return JsonToPy(self.ToJson()); },
           "The full result -- every box, every placed item, and every "
           "unplaced item -- as a plain Python dict.")
      .def("to_json", [](const PackResponse& self, int indent) { return self.Dump(indent); },
           py::arg("indent") = 2, "The full result as a JSON string.")
      .def("__repr__", [](const PackResponse& self) {
        return "<fitsolver.PackResult success=" + std::string(self.success ? "True" : "False") +
               " boxes=" + std::to_string(self.boxes.Count()) +
               " unplaced=" + std::to_string(self.unplaced_items.Count()) + ">";
      });

  m.def("solve", &SolveJson, py::arg("request_json"),
        "The simplest entry point: pass a JSON string shaped like\n"
        "{\"items\": [...], \"boxes\": [...]} and get a JSON string back.\n"
        "No fitsolver types required on either side.");

  m.def("pack_dicts", &PackDicts, py::arg("items"), py::arg("boxes"),
        "Like solve(), but takes/returns native Python objects: pass lists\n"
        "of plain dicts shaped like items.json / boxes.json and get back a\n"
        "rich PackResult with attribute access to every feature.");

  m.def("pack", &PackObjects, py::arg("items"), py::arg("boxes"),
        "Like pack_dicts(), but takes lists of fitsolver.Item / fitsolver.Box\n"
        "objects instead of plain dicts.");
}