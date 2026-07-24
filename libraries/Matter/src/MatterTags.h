// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <MatterEndPoint.h>

// Named Matter semantic tags for use with MatterEndPoint::setTagList(), so sketches don't need to
// hardcode namespace/tag numbers from the Matter "Standard Namespaces" specification:
// https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces
// Any endpoint accepts any of these tags — they're presets, not a gate tied to a specific device type.
// Use MatterTags::make() for a tag outside these namespaces, or a custom label.
namespace MatterTags {

// Creates a custom MatterTag from a namespace ID, tag value and optional label.
constexpr MatterTag make(uint8_t namespaceId, uint8_t tag, const char *label = nullptr) {
  return MatterTag{namespaceId, tag, label};
}

namespace Position {
static constexpr uint8_t NS = 0x08;
static constexpr MatterTag Left = {NS, 0};
static constexpr MatterTag Right = {NS, 1};
static constexpr MatterTag Top = {NS, 2};
static constexpr MatterTag Bottom = {NS, 3};
static constexpr MatterTag Middle = {NS, 4};
}  // namespace Position

namespace Number {
static constexpr uint8_t NS = 0x07;
static constexpr MatterTag One = {NS, 1};
static constexpr MatterTag Two = {NS, 2};
static constexpr MatterTag Three = {NS, 3};
static constexpr MatterTag Four = {NS, 4};
static constexpr MatterTag Five = {NS, 5};
}  // namespace Number

namespace Switches {
static constexpr uint8_t NS = 0x43;
static constexpr MatterTag On = {NS, 0};
static constexpr MatterTag Off = {NS, 1};
static constexpr MatterTag Toggle = {NS, 2};
static constexpr MatterTag Up = {NS, 3};
static constexpr MatterTag Down = {NS, 4};
static constexpr MatterTag Custom = {NS, 8};  // use together with a custom .label
}  // namespace Switches

namespace Location {
static constexpr uint8_t NS = 0x06;
static constexpr MatterTag Indoor = {NS, 0};
static constexpr MatterTag Outdoor = {NS, 1};
}  // namespace Location

}  // namespace MatterTags
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
