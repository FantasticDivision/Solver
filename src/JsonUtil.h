// Small shared helpers for parsing the optional fields in the request
// JSON. Header-only and internal -- not part of the public model.
#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace fitsolver::json_detail {

inline double GetOptionalNumber(const nlohmann::json& j, const char* key, double fallback) {
  if (!j.contains(key) || j.at(key).is_null()) return fallback;
  return j.at(key).get<double>();
}

inline std::string GetOptionalString(const nlohmann::json& j, const char* key,
                                      const std::string& fallback) {
  if (!j.contains(key) || j.at(key).is_null()) return fallback;
  return j.at(key).get<std::string>();
}

inline bool GetOptionalBool(const nlohmann::json& j, const char* key, bool fallback) {
  if (!j.contains(key) || j.at(key).is_null()) return fallback;
  return j.at(key).get<bool>();
}

// Renders `value` as JSON null when it equals the sentinel used for
// "no limit" (e.g. Box::kNoWeightLimit), otherwise as the number itself.
inline nlohmann::json NumberOrNull(double value, double sentinel) {
  if (value < 0.0 && value == sentinel) return nullptr;
  return value;
}

}  // namespace fitsolver::json_detail
