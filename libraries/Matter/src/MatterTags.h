// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief Named Matter semantic tags for @ref MatterEndPoint::setTagList.
 *
 * Presets so sketches do not hardcode namespace/tag numbers from the Matter
 * Standard Namespaces specification. Any endpoint accepts any of these tags —
 * they are not gated to a specific device type.
 *
 * Use @ref createTag for a tag outside these namespaces, or a custom label.
 * For the Switches Custom tag, use @ref Switches::createCustomTag. For Position
 * Row/Column (non-empty label required), use @ref Position::createRowTag /
 * @ref Position::createColumnTag.
 */
namespace MatterTags {

/**
 * @brief Create a custom MatterTag from a namespace ID, tag value, and optional label.
 * @param namespaceId Matter namespace identifier.
 * @param tag Tag value within that namespace.
 * @param label Optional label; not copied, must outlive the endpoint. nullptr means no label.
 * @return MatterTag suitable for @ref MatterEndPoint::setTagList.
 */
constexpr MatterTag createTag(uint8_t namespaceId, uint8_t tag, const char *label = nullptr) {
  return MatterTag{namespaceId, tag, label};
}

/**
 * @brief Position namespace (0x08): Left, Right, Top, Bottom, Middle, Row, Column.
 */
namespace Position {
/**
 * @brief CHIP Globals::PositionTag (same values as the Position presets).
 */
using PositionTag = chip::app::Clusters::Globals::PositionTag;
static constexpr uint8_t NS = 0x08;  ///< Position namespace ID.
static constexpr MatterTag Left = {NS, (uint8_t)PositionTag::kLeft};      ///< Position Left.
static constexpr MatterTag Right = {NS, (uint8_t)PositionTag::kRight};    ///< Position Right.
static constexpr MatterTag Top = {NS, (uint8_t)PositionTag::kTop};        ///< Position Top.
static constexpr MatterTag Bottom = {NS, (uint8_t)PositionTag::kBottom};  ///< Position Bottom.
static constexpr MatterTag Middle = {NS, (uint8_t)PositionTag::kMiddle};  ///< Position Middle.
static constexpr uint8_t Row = (uint8_t)PositionTag::kRow;        ///< Row tag value; requires a non-empty label.
static constexpr uint8_t Column = (uint8_t)PositionTag::kColumn;  ///< Column tag value; requires a non-empty label.
/**
 * @brief Create a Position Row tag. The Matter spec uses an Arabic numeral such as "1" for the first row.
 * @param label Non-empty label; must outlive the endpoint (a string literal is fine).
 * @return MatterTag for setTagList().
 */
constexpr MatterTag createRowTag(const char *label) {
  return MatterTag{NS, Row, label};
}
/**
 * @brief Create a Position Column tag. The Matter spec uses an Arabic numeral such as "1" for the first column.
 * @param label Non-empty label; must outlive the endpoint (a string literal is fine).
 * @return MatterTag for setTagList().
 */
constexpr MatterTag createColumnTag(const char *label) {
  return MatterTag{NS, Column, label};
}
}  // namespace Position

/**
 * @brief Number namespace (0x07): Zero through Ten.
 */
namespace Number {
static constexpr uint8_t NS = 0x07;  ///< Number namespace ID.
static constexpr MatterTag Zero = {NS, 0};     ///< Number 0.
static constexpr MatterTag One = {NS, 1};      ///< Number 1.
static constexpr MatterTag Two = {NS, 2};      ///< Number 2.
static constexpr MatterTag Three = {NS, 3};    ///< Number 3.
static constexpr MatterTag Four = {NS, 4};     ///< Number 4.
static constexpr MatterTag Five = {NS, 5};     ///< Number 5.
static constexpr MatterTag Six = {NS, 6};      ///< Number 6.
static constexpr MatterTag Seven = {NS, 7};    ///< Number 7.
static constexpr MatterTag Eight = {NS, 8};    ///< Number 8.
static constexpr MatterTag Nine = {NS, 9};     ///< Number 9.
static constexpr MatterTag Ten = {NS, 0x0A};   ///< Number 10.
}  // namespace Number

/**
 * @brief Switches namespace (0x43): On, Off, Toggle, Up, Down, Next, Previous, Select, Custom.
 */
namespace Switches {
static constexpr uint8_t NS = 0x43;     ///< Switches namespace ID.
static constexpr uint8_t Custom = 8;    ///< Custom tag value; requires a non-empty label.
static constexpr MatterTag On = {NS, 0};         ///< Switches On.
static constexpr MatterTag Off = {NS, 1};        ///< Switches Off.
static constexpr MatterTag Toggle = {NS, 2};     ///< Switches Toggle.
static constexpr MatterTag Up = {NS, 3};         ///< Switches Up.
static constexpr MatterTag Down = {NS, 4};       ///< Switches Down.
static constexpr MatterTag Next = {NS, 5};       ///< Switches Next.
static constexpr MatterTag Previous = {NS, 6};   ///< Switches Previous.
static constexpr MatterTag Select = {NS, 7};     ///< Spec name: Enter/OK/Select.
/**
 * @brief Create a Switches Custom tag with a user-visible label.
 * @param label Non-empty label; must outlive the endpoint (a string literal is fine).
 * @return MatterTag for setTagList().
 */
constexpr MatterTag createCustomTag(const char *label) {
  return MatterTag{NS, Custom, label};
}
}  // namespace Switches

/**
 * @brief Location namespace (0x06): Indoor, Outdoor, Inside, Outside.
 */
namespace Location {
/**
 * @brief CHIP Globals::LocationTag (same values as the Location presets).
 */
using LocationTag = chip::app::Clusters::Globals::LocationTag;
static constexpr uint8_t NS = 0x06;  ///< Location namespace ID.
static constexpr MatterTag Indoor = {NS, (uint8_t)LocationTag::kIndoor};    ///< Location Indoor.
static constexpr MatterTag Outdoor = {NS, (uint8_t)LocationTag::kOutdoor};  ///< Location Outdoor.
static constexpr MatterTag Inside = {NS, (uint8_t)LocationTag::kInside};    ///< Location Inside.
static constexpr MatterTag Outside = {NS, (uint8_t)LocationTag::kOutside};  ///< Location Outside.
}  // namespace Location

}  // namespace MatterTags
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
