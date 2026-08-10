// Validation test for the btInUse() strong-override backwards-compatibility path.
//
// This sketch deliberately does NOT include esp32-hal-alloc-ble-mem.h or
// esp32-hal-alloc-bt-classic-mem.h, so bleInUse() and btClassicInUse() both
// return false. Instead it provides a strong btInUse() returning true, exactly
// as legacy user code would. initArduino() must detect the strong override via
// the pointer comparison and skip all memory releases.

#include <Arduino.h>
#include "esp32-hal-bt.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if SOC_BT_SUPPORTED && (defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)) \
  && __has_include("esp_bt.h") && defined(CONFIG_BT_CONTROLLER_ENABLED)

// Strong override — simulates legacy user code keeping all BT memory.
// bleInUse() and btClassicInUse() are both false (no alloc headers included),
// so this is the ONLY thing that should prevent memory from being released.
bool btInUse(void) {
  return true;
}

static int pass_count = 0;
static int fail_count = 0;

static void check(bool ok, const char *desc) {
  if (ok) {
    Serial.printf("[BT_INUSE_OVERRIDE] PASS: %s\n", desc);
    pass_count++;
  } else {
    Serial.printf("[BT_INUSE_OVERRIDE] FAIL: %s\n", desc);
    fail_count++;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[BT_INUSE_OVERRIDE] Ready");

  // Verify the pointer comparison correctly identifies the strong override.
  extern bool _btInUse_default(void);
  check((void *)btInUse != (void *)_btInUse_default, "btInUse does NOT point to default (strong override detected)");

  // Sub-functions return false (no alloc headers) — the strong btInUse() override
  // is the only reason initArduino() should have kept memory.
  check(!bleInUse(), "bleInUse() is false (no alloc-ble-mem header included)");
  check(!btClassicInUse(), "btClassicInUse() is false (no alloc-bt-classic-mem header included)");

  // Neither mode's memory should have been released by initArduino().
  check(!btMemReleased(BT_MODE_BLE), "BLE mem NOT released despite bleInUse=false (btInUse override protected it)");
#if CONFIG_BT_CLASSIC_ENABLED
  check(!btMemReleased(BT_MODE_CLASSIC_BT), "Classic BT mem NOT released despite btClassicInUse=false (btInUse override protected it)");
#else
  check(btMemReleased(BT_MODE_CLASSIC_BT), "Classic BT mem reported released (not supported on this chip)");
#endif

  // Confirm BT is actually startable — memory is intact.
  check(btStart(), "btStart() succeeds (memory was preserved by btInUse override)");
  check(btStarted(), "btStarted() true");
  check(btStop(), "btStop() succeeds");

  Serial.printf("[BT_INUSE_OVERRIDE] %d passed, %d failed\n", pass_count, fail_count);
  Serial.printf("[BT_INUSE_OVERRIDE] %s\n", (fail_count == 0) ? "PASSED" : "FAILED");
}

void loop() {}

#else

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("[BT_INUSE_OVERRIDE] SKIP: BT controller not available");
}

void loop() {}

#endif
