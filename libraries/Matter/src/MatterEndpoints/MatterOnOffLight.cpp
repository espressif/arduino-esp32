#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterOnOffLight.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterOnOffLight::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter On-Off Light device has not begun.");
    return false;
  }
  
  log_d("OnOff Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);

  if (endpoint_id == getEndPointId()) {
    if (cluster_id == OnOff::Id) {
      if (attribute_id == OnOff::Attributes::OnOff::Id) {
        if (_onChangeCB != NULL) {
          ret = _onChangeCB(val->val.b);
          log_d("OnOffLight state changed to %d", val->val.b);
          if (ret == true) {
            state = val->val.b;
          }
        }
      }
    }
  }
  return ret;
}

MatterOnOffLight::MatterOnOffLight() {}

MatterOnOffLight::~MatterOnOffLight() {
  end();
}

bool MatterOnOffLight::begin(bool initialState) {
  ArduinoMatter::_init();
  on_off_light::config_t light_config;

  light_config.on_off.on_off = initialState;
  light_config.on_off.lighting.start_up_on_off = nullptr;
  state = initialState;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = on_off_light::create(node::get(), &light_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create on-off light endpoint");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("On-Off Light created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterOnOffLight::end() {
  started = false;
}

bool MatterOnOffLight::setOnOff(bool newState) {
  if (!started) {
    log_e("Matter On-Off Light device has not begun.");
    return false;
  }

  // avoid processing the a "no-change"
  if (state == newState) {
    return true;
  }

  state = newState;

  endpoint_t *endpoint = endpoint::get(node::get(), endpoint_id);
  cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
  attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);

  if (val.val.b != state) {
    val.val.b = state;
    attribute::update(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
  }
  return true;
}

bool MatterOnOffLight::getOnOff() {
  return state;
}

bool MatterOnOffLight::toggle() {
  return setOnOff(!state);
}

MatterOnOffLight::operator bool() {
  return getOnOff();
}

void MatterOnOffLight::operator=(bool newState) {
  setOnOff(newState);
}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
