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
#include <app-common/zap-generated/cluster-enums.h>

// Named Matter semantic tags for use with MatterEndPoint::setTagList(), so sketches don't need to
// hardcode namespace/tag numbers from the Matter "Standard Namespaces" specification:
// https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces
// Any endpoint accepts any of these tags — they're presets, not a gate tied to a specific device type.
// Use MatterTags::createTag() for a tag outside these namespaces, or a custom label.
// For the Switches Custom tag, use MatterTags::Switches::createCustomTag(label).
// For Position Row/Column (label required), use MatterTags::Position::createRowTag() / createColumnTag().
// Position and Location tag values use CHIP Globals::PositionTag / LocationTag (same pattern as
// MatterFan's FanModeEnum). Number, Switches, and namespace IDs have no CHIP constants in this SDK.
namespace MatterTags {

// Creates a custom MatterTag from a namespace ID, tag value and optional label.
constexpr MatterTag createTag(uint8_t namespaceId, uint8_t tag, const char *label = nullptr) {
  return MatterTag{namespaceId, tag, label};
}

namespace Position {
using PositionTag = chip::app::Clusters::Globals::PositionTag;
static constexpr uint8_t NS = 0x08;
static constexpr MatterTag Left = {NS, (uint8_t)PositionTag::kLeft};
static constexpr MatterTag Right = {NS, (uint8_t)PositionTag::kRight};
static constexpr MatterTag Top = {NS, (uint8_t)PositionTag::kTop};
static constexpr MatterTag Bottom = {NS, (uint8_t)PositionTag::kBottom};
static constexpr MatterTag Middle = {NS, (uint8_t)PositionTag::kMiddle};
// Row and Column require a numeric label (e.g. "1"); the string must outlive the endpoint.
static constexpr uint8_t Row = (uint8_t)PositionTag::kRow;
static constexpr uint8_t Column = (uint8_t)PositionTag::kColumn;
constexpr MatterTag createRowTag(const char *label) {
  return MatterTag{NS, Row, label};
}
constexpr MatterTag createColumnTag(const char *label) {
  return MatterTag{NS, Column, label};
}
}  // namespace Position

namespace Number {
static constexpr uint8_t NS = 0x07;
static constexpr MatterTag Zero = {NS, 0};
static constexpr MatterTag One = {NS, 1};
static constexpr MatterTag Two = {NS, 2};
static constexpr MatterTag Three = {NS, 3};
static constexpr MatterTag Four = {NS, 4};
static constexpr MatterTag Five = {NS, 5};
static constexpr MatterTag Six = {NS, 6};
static constexpr MatterTag Seven = {NS, 7};
static constexpr MatterTag Eight = {NS, 8};
static constexpr MatterTag Nine = {NS, 9};
static constexpr MatterTag Ten = {NS, 0x0A};
}  // namespace Number

namespace Switches {
static constexpr uint8_t NS = 0x43;
static constexpr uint8_t Custom = 8;  // requires a non-empty label
static constexpr MatterTag On = {NS, 0};
static constexpr MatterTag Off = {NS, 1};
static constexpr MatterTag Toggle = {NS, 2};
static constexpr MatterTag Up = {NS, 3};
static constexpr MatterTag Down = {NS, 4};
static constexpr MatterTag Next = {NS, 5};
static constexpr MatterTag Previous = {NS, 6};
static constexpr MatterTag Select = {NS, 7};  // spec name: Enter/OK/Select
// The string must outlive the endpoint (a literal is fine).
constexpr MatterTag createCustomTag(const char *label) {
  return MatterTag{NS, Custom, label};
}
}  // namespace Switches

namespace Location {
using LocationTag = chip::app::Clusters::Globals::LocationTag;
static constexpr uint8_t NS = 0x06;
static constexpr MatterTag Indoor = {NS, (uint8_t)LocationTag::kIndoor};
static constexpr MatterTag Outdoor = {NS, (uint8_t)LocationTag::kOutdoor};
static constexpr MatterTag Inside = {NS, (uint8_t)LocationTag::kInside};
static constexpr MatterTag Outside = {NS, (uint8_t)LocationTag::kOutside};
}  // namespace Location

}  // namespace MatterTags
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
