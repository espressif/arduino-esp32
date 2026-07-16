#pragma once

// GCC 14 + C++20 requires same-type operator== for std::optional<T> comparison.
// ClosureControl structs only define operator==(const BaseType&), which is not
// sufficient. This header adds free-function operators via ADL without modifying
// managed_components.
//
// Included from Matter.h for user code that compares ClosureControl Nullable types.
// Also injected into espressif__esp_matter via CMake -include when Matter is enabled.

#ifdef __cplusplus

#include "app/clusters/closure-control-server/closure-control-cluster-objects.h"

namespace chip {
namespace app {
namespace Clusters {
namespace ClosureControl {

inline bool operator==(const GenericOverallCurrentState &a, const GenericOverallCurrentState &b) {
  return a.position == b.position && a.latch == b.latch && a.speed == b.speed && a.secureState == b.secureState;
}

inline bool operator==(const GenericOverallTargetState &a, const GenericOverallTargetState &b) {
  return a.position == b.position && a.latch == b.latch && a.speed == b.speed;
}

}  // namespace ClosureControl
}  // namespace Clusters
}  // namespace app
}  // namespace chip

#endif  // __cplusplus
