// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <sdkconfig.h>
#include <MatterC5Network.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Arduino.h>
#include <esp_matter.h>
#include <functional>
#include <initializer_list>

namespace chip {
namespace app {
class ServerClusterInterface;
}
}  // namespace chip

using namespace esp_matter;

class ArduinoMatter;

/**
 * @brief A single Matter semantic tag (Descriptor cluster TagList entry).
 *
 * Tags disambiguate sibling endpoints that expose the same device type, or
 * otherwise clarify an endpoint's role/position (for example tagging 3 buttons
 * with Number and Position tags so a controller can tell them apart).
 *
 * @c namespaceId / @c tag values come from the Matter Standard Namespaces
 * specification. See MatterTags.h for named constants and
 * @c MatterTags::createTag() for a custom namespace/tag/label combination.
 */
struct MatterTag {
  uint8_t namespaceId;           ///< Matter namespace identifier.
  uint8_t tag;                   ///< Tag value within that namespace.
  const char *label = nullptr;   ///< Optional label; nullptr means no label. Not copied; must outlive the endpoint.
};

/**
 * @brief Last Identify cluster event for an endpoint.
 *
 * Filled immediately before @ref MatterEndPoint::onIdentify runs. Call
 * @ref MatterEndPoint::getIdentifyRequest from that callback (or later) to
 * distinguish IdentifyTime from TriggerEffect and to read the effect id.
 */
struct MatterIdentifyRequest {
  /**
   * @brief Matter Identify::TriggerEffect EffectIdentifierEnum values.
   *
   * Meaningful when @ref fromTriggerEffect is true. On IdentifyTime START/STOP,
   * CHIP may still pass a leftover/default id (often Blink); ignore it then.
   */
  enum EffectId : uint8_t {
    BLINK = 0x00,           ///< Blink effect.
    BREATHE = 0x01,         ///< Breathe effect.
    OKAY = 0x02,            ///< Okay / confirmation effect.
    CHANNEL_CHANGE = 0x0B,  ///< Channel change effect.
    FINISH = 0xFE,          ///< FinishEffect.
    STOP = 0xFF             ///< StopEffect.
  };

  bool valid = false;              ///< false until this endpoint has received an Identify event. Do not treat a default effectId of 0 as Blink.
  bool active = false;             ///< Same boolean passed to onIdentify().
  uint8_t effectId = 0;            ///< EffectId; meaningful when fromTriggerEffect is true.
  uint8_t effectVariant = 0;       ///< Usually Default (0).
  bool fromTriggerEffect = false;  ///< true for TriggerEffect; false for Identify / IdentifyTime.
};

/**
 * @brief Base class for all Matter endpoints.
 *
 * Provides endpoint id management, cluster attribute get/set/update, Identify
 * cluster support (visual feedback), Descriptor TagList semantic tags, and the
 * attribute-change callback framework. All stock @c Matter* endpoint classes
 * inherit from this class.
 *
 * For esp-matter device types that are not wrapped by a stock class, subclass
 * this type, implement @ref attributeChangeCB, and in subclass @c begin() call
 * @c ensureMatterNode() (protected), create the endpoint with
 * @c esp_matter::endpoint::*::create(..., (void *)this), then
 * @ref setEndPointId or @ref registerCreatedEndpoint.
 */
class MatterEndPoint {
public:
  /**
   * @brief Attribute write flavor used by helper methods.
   */
  enum attrOperation_t {
    ATTR_SET = false,     ///< Write via set (controller-writable path).
    ATTR_UPDATE = true    ///< Update via update (device-originated / read-only path).
  };

  /**
   * @brief Callback for Identify cluster activity.
   * @param identifyIsEnabled true to start visual feedback; false to stop.
   * @return true if the change was handled successfully.
   */
  using EndPointIdentifyCB = std::function<bool(bool)>;

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Override in a subclass (stock endpoints already do). Sketches typically do
   * not override this unless custom behavior is needed.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully, false otherwise.
   */
  virtual bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) = 0;

  /**
   * @brief Deprecated secondary Network Commissioning helper.
   *
   * Arduino Matter exposes one Network Commissioning cluster on endpoint 0
   * (Wi-Fi or Thread via @c Matter.selectNetwork()), not both. Does not create
   * an endpoint and always returns false.
   *
   * @return Always false.
   */
  [[deprecated("Use Matter.selectNetwork(MATTER_NETWORK_WIFI or MATTER_NETWORK_THREAD); Network Commissioning is on endpoint 0")]]
  bool createSecondaryNetworkInterface();

  /**
   * @brief Deprecated secondary Network Commissioning endpoint id.
   *
   * Always 0; Network Commissioning is on endpoint 0.
   *
   * @return Always 0.
   */
  [[deprecated("Network Commissioning is on endpoint 0; this always returns 0")]]
  uint16_t getSecondaryNetworkEndPointId();

  /**
   * @brief Get the current Matter accessory endpoint ID.
   * @return Endpoint number (typically 1–254).
   */
  uint16_t getEndPointId();

  /**
   * @brief Set the current Matter accessory endpoint ID.
   * @param ep Endpoint number to set.
   */
  void setEndPointId(uint16_t ep);

  /**
   * @brief Get a pointer to an attribute from its cluster ID and attribute ID.
   * @param cluster_id Cluster ID (e.g. OnOff::Id).
   * @param attribute_id Attribute ID (e.g. OnOff::Attributes::OnOff::Id).
   * @return Pointer to the attribute, or NULL if not found.
   */
  esp_matter::attribute_t *getAttribute(uint32_t cluster_id, uint32_t attribute_id);

  /**
   * @brief Get the value of an attribute from its cluster ID and attribute ID.
   * @param cluster_id Cluster ID.
   * @param attribute_id Attribute ID.
   * @param attrVal Pointer to store the attribute value.
   * @return true if successful, false otherwise.
   */
  bool getAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

  /**
   * @brief Set the value of an attribute from its cluster ID and attribute ID.
   * @param cluster_id Cluster ID.
   * @param attribute_id Attribute ID.
   * @param attrVal Pointer to the attribute value to set.
   * @return true if successful, false otherwise.
   */
  bool setAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

  /**
   * @brief Update the value of an attribute (typically a read-only, device-originated value).
   *
   * Boolean State @c StateValue (contact, leak, freeze, rain) lives on the
   * code-driven cluster and cannot be written with this method. Use the
   * endpoint setters instead.
   *
   * @param cluster_id Cluster ID.
   * @param attribute_id Attribute ID.
   * @param attrVal Pointer to the attribute value to update.
   * @return true if successful, false otherwise.
   */
  bool updateAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

  /**
   * @brief Identify cluster entry point used by the Matter stack.
   *
   * Stores @a request, then calls the user @ref onIdentify callback with
   * @c request.active.
   *
   * @param endpoint_id Endpoint that received the Identify event.
   * @param request Identify request details.
   * @return true if handled successfully.
   */
  bool endpointIdentifyCB(uint16_t endpoint_id, const MatterIdentifyRequest &request);

  /**
   * @brief Register a callback for the Identify cluster.
   *
   * Invoked when clients interact with Identify on this endpoint. When the
   * boolean is true, provide visual feedback (e.g. blink an LED); when false,
   * stop. Both IdentifyTime START/STOP and TriggerEffect invoke this callback.
   * TriggerEffect Blink/Breathe/Okay/ChannelChange report true; StopEffect and
   * FinishEffect report false. A one-shot TriggerEffect does not send a later
   * STOP; if the sketch latches a flag, time the animation out.
   *
   * @param onEndPointIdentifyCB Callback; receives whether identify is enabled.
   */
  void onIdentify(EndPointIdentifyCB onEndPointIdentifyCB);

  /**
   * @brief Last Identify event for this endpoint.
   *
   * Updated immediately before the @ref onIdentify callback runs. Use it to
   * distinguish IdentifyTime from TriggerEffect and to read the effect id.
   *
   * @return Copy of the last Identify request (valid is false until the first event).
   */
  MatterIdentifyRequest getIdentifyRequest() const;

  /**
   * @brief Maximum number of Descriptor TagList entries per endpoint.
   *
   * Matches esp-matter @c ESP_MATTER_MAX_SEMANTIC_TAG_COUNT.
   */
  static constexpr uint8_t MAX_TAG_LIST_SIZE = 3;

  /**
   * @brief Set the Descriptor cluster TagList, replacing any list set previously.
   *
   * Enables the TagList feature on first use. Call after the endpoint @c begin()
   * and before @c Matter.begin(). At most @ref MAX_TAG_LIST_SIZE entries.
   * Switches Custom and Position Row/Column tags require a non-empty label.
   * Optional @c label pointers are not copied and must remain valid while the
   * endpoint is running (string literals are fine).
   *
   * @param tagList Array of MatterTag entries.
   * @param count Number of entries; must be 1..3.
   * @return true if successful, false otherwise (including if begin() has not been called).
   */
  bool setTagList(const MatterTag *tagList, uint8_t count);

  /**
   * @brief Set the Descriptor cluster TagList from a brace-enclosed list.
   *
   * Convenience overload: @c Light1.setTagList({MatterTags::Position::Top, MatterTags::Number::One}).
   *
   * @param tagList Initializer list of MatterTag entries (1..3).
   * @return true if successful, false otherwise.
   */
  bool setTagList(std::initializer_list<MatterTag> tagList);

  /**
   * @brief Register an endpoint created with @c esp_matter::endpoint::*::create(..., (void *)this).
   *
   * Use this in a custom @c MatterEndPoint subclass when not using a stock
   * @c Matter* @c begin(). Call before @c Matter.begin().
   *
   * @param ep Endpoint created by esp-matter.
   * @return true if registered, false otherwise.
   */
  bool registerCreatedEndpoint(endpoint_t *ep);

protected:
  friend class ArduinoMatter;

  // Creates the Matter node once (root endpoint 0). Idempotent. Used by stock and custom begin().
  static bool ensureMatterNode();

  // Called by ArduinoMatter after esp_matter::start(). Code-driven cluster objects exist now.
  void notifyStackStarted();

  // used for secondary network interface endpoints
  static uint16_t secondary_network_endpoint_id;
  // main endpoint ID
  uint16_t endpoint_id = 0;
  EndPointIdentifyCB _onEndPointIdentifyCB = nullptr;
  MatterIdentifyRequest identifyRequest;
  bool tagListEnabled = false;
  bool pendingBooleanState = false;
  bool hasPendingBooleanState = false;

  /**
   * @brief Enable the Descriptor cluster TagList feature on this endpoint.
   *
   * Called automatically by @ref setTagList. Idempotent. Subclasses that want
   * TagList advertised even when the sketch never calls setTagList() (Generic
   * Switch) may call this from begin() after setEndPointId().
   *
   * @return true if TagList is enabled.
   */
  bool enableTagList();

  /**
   * @brief Set BooleanState::StateValue on the code-driven cluster.
   *
   * @c attribute::update() only writes the shadow table. Before Matter.begin()
   * the value is cached and pushed when the stack starts. Endpoint setters such
   * as setContact() / setLeak() / setFreeze() / setRain() use this helper.
   *
   * @param value New StateValue.
   * @return true if stored or written successfully.
   */
  bool setBooleanStateValue(bool value);

  /**
   * @brief Hook called once after Matter.begin() when code-driven clusters exist.
   *
   * Override in a subclass to push cached attribute values after
   * @c esp_matter::start() succeeds. The library invokes this via an internal
   * notifyStackStarted(); do not call that from application code. Default is a
   * no-op.
   */
  virtual void onStackStarted() {}

  /**
   * @brief Live CHIP cluster for this endpoint.
   * @param cluster_id Cluster ID to look up.
   * @return Cluster interface, or nullptr before Matter.begin().
   */
  chip::app::ServerClusterInterface *findRegisteredCluster(uint32_t cluster_id);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
