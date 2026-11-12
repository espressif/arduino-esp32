// Validation test for the --wrap linker intercept on esp_bt_mem_release /
// esp_bt_controller_mem_release (see cores/esp32/esp32-hal-bt.c).
//
// The test is split into phases, each driven by the Python harness via serial
// after a hard reset, so every phase starts with a fully fresh BT memory state.
//
//   Phase 1 — btMemRelease() Arduino API
//     Verifies that the Arduino API succeeds, updates the tracking flag, and
//     that a second call is a safe no-op.
//
//   Phase 2 — direct esp_bt_controller_mem_release()
//     Simulates an external component (e.g. the Matter stack) calling the
//     ESP-IDF function directly.  The __wrap_esp_bt_controller_mem_release
//     intercept must update the tracking flag and guard against double-release.
//
//   Phase 3 — direct esp_bt_mem_release()
//     Same as phase 2 but for the combined host+controller release path
//     intercepted by __wrap_esp_bt_mem_release.
//
//   Phase 4 — cross-API double-free (btMemRelease → esp_bt_controller_mem_release)
//     Releases via the Arduino API first, then calls esp_bt_controller_mem_release()
//     directly.  Both wraps share the same tracking flags, so the second call
//     must be a no-op regardless of which API is used.
//
//   Phase 5 — cross-API double-free (esp_bt_mem_release → btMemRelease)
//     Releases via direct esp_bt_mem_release() first, then via the Arduino API.
//     Mirrors phase 4 in the opposite order.
//
//   Phase 6 — btMarkMemReleased() sentinel
//     Marks memory as released via btMarkMemReleased() without performing a
//     real release (simulating a ROM or pre-wrap call that bypassed the
//     intercept).  All subsequent wrap calls must be no-ops.
//
//   Phase 7 — btStartMode(BLE) fails after btMemRelease(BLE)
//     Verifies that once BLE memory has been freed, attempting to start BLE
//     fails gracefully.  On BTDM-capable chips btStart() may still succeed by
//     downgrading to Classic-only mode (see phase 9 for the mirror case).
//
//   Phase 8 — full lifecycle: start, release-while-running (rejected), stop, release
//     Starts BT to confirm the memory is intact, then attempts btMemRelease()
//     while the controller is still active (must fail without updating tracking),
//     stops BT cleanly, and finally releases memory successfully.
//
//   Phase 9 — Matter-style release (Classic BT)
//     Simulates the Matter stack calling esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)
//     during its BLE initialization to free the unused Classic BT controller memory before
//     starting BLE commissioning.
//
// The Python test sends "PHASE N\n" over serial after each hard reset and
// checks the pass/fail output.

#include <Arduino.h>
#include "esp32-hal-bt.h"
// Including esp32-hal-alloc-bt-classic-mem.h and esp32-hal-alloc-ble-mem.h sets
// _btClassicLibraryInUse and _bleLibraryInUse to true via constructors,
// which prevents initArduino() from auto-releasing BT memory before setup()
// runs. Without this, every phase would start with memory already released.
#include "esp32-hal-alloc-bt-classic-mem.h"
#include "esp32-hal-alloc-ble-mem.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if SOC_BT_SUPPORTED && (defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)) \
  && __has_include("esp_bt.h") && defined(CONFIG_BT_CONTROLLER_ENABLED)

#include "esp_bt.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include "esp_bt_main.h"
#endif

static int pass_count = 0;
static int fail_count = 0;

static void check(bool ok, const char *desc) {
  if (ok) {
    Serial.printf("[BT_MEM_WRAP] PASS: %s\n", desc);
    pass_count++;
  } else {
    Serial.printf("[BT_MEM_WRAP] FAIL: %s\n", desc);
    fail_count++;
  }
}

// Phase 1: btMemRelease() internally calls esp_bt_mem_release() which is
// intercepted by __wrap_esp_bt_mem_release.  Verify the tracking flag is
// updated and that a second call is a harmless no-op.
static void phase_1() {
  Serial.println("[BT_MEM_WRAP] Phase 1: btMemRelease API");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");
  check(btMemRelease(BT_MODE_BLE), "btMemRelease(BLE) succeeds");
  check(btMemReleased(BT_MODE_BLE), "tracking updated after btMemRelease");

  // Second call: __wrap_esp_bt_mem_release strips the already-released BLE
  // bit, skips the real call and returns ESP_OK.  btMemRelease() maps that
  // to true.
  check(btMemRelease(BT_MODE_BLE), "second btMemRelease is a no-op (returns true)");
  check(btMemReleased(BT_MODE_BLE), "tracking still true after double call");
}

// Phase 2: call esp_bt_controller_mem_release() directly, as an external
// component would.  __wrap_esp_bt_controller_mem_release must update the
// tracking flag and guard against a double-release.
static void phase_2() {
  Serial.println("[BT_MEM_WRAP] Phase 2: direct esp_bt_controller_mem_release");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");

  esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "esp_bt_controller_mem_release returns ESP_OK");
  check(btMemReleased(BT_MODE_BLE), "wrap updated tracking after controller_mem_release");

  // Double-release guard: wrap strips already-released bits, skips real call.
  ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "double esp_bt_controller_mem_release is a no-op (ESP_OK)");
  check(btMemReleased(BT_MODE_BLE), "tracking still true after double controller call");
}

// Phase 3: call esp_bt_mem_release() directly (host + controller combined).
// __wrap_esp_bt_mem_release must update the tracking flag and guard against
// a double-release.
static void phase_3() {
  Serial.println("[BT_MEM_WRAP] Phase 3: direct esp_bt_mem_release");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");

  esp_err_t ret = esp_bt_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "esp_bt_mem_release returns ESP_OK");
  check(btMemReleased(BT_MODE_BLE), "wrap updated tracking after bt_mem_release");

  // Double-release guard.
  ret = esp_bt_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "double esp_bt_mem_release is a no-op (ESP_OK)");
  check(btMemReleased(BT_MODE_BLE), "tracking still true after double bt_mem_release");
}

// Phase 4: cross-API double-free — release via the Arduino btMemRelease() API
// first, then call esp_bt_controller_mem_release() directly.  Although the two
// paths go through different __wrap functions, they share the same tracking
// flags, so the second call must detect the already-released state and return
// ESP_OK without touching memory.
static void phase_4() {
  Serial.println("[BT_MEM_WRAP] Phase 4: cross-API double-free (btMemRelease then esp_bt_controller_mem_release)");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");

  // First release via the Arduino API (uses __wrap_esp_bt_mem_release internally).
  check(btMemRelease(BT_MODE_BLE), "btMemRelease(BLE) succeeds");
  check(btMemReleased(BT_MODE_BLE), "tracking updated after btMemRelease");

  // Second release via the other direct ESP-IDF path: __wrap_esp_bt_controller_mem_release
  // checks the same flag and must skip the real call.
  esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "esp_bt_controller_mem_release after btMemRelease is ESP_OK (no-op)");
  check(btMemReleased(BT_MODE_BLE), "tracking still true after cross-API double-free");
}

// Phase 5: cross-API double-free (reversed order) — release via direct
// esp_bt_mem_release() first, then call btMemRelease().  The Arduino API must
// recognize that the tracking flag is already set and return true without
// attempting a second real release.
static void phase_5() {
  Serial.println("[BT_MEM_WRAP] Phase 5: cross-API double-free (esp_bt_mem_release then btMemRelease)");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");

  // First release via the direct ESP-IDF path (__wrap_esp_bt_mem_release).
  esp_err_t ret = esp_bt_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "direct esp_bt_mem_release succeeds");
  check(btMemReleased(BT_MODE_BLE), "tracking updated by wrap");

  // Second release via the Arduino API: sees the flag already set → no-op.
  check(btMemRelease(BT_MODE_BLE), "btMemRelease after esp_bt_mem_release is true (no-op)");
  check(btMemReleased(BT_MODE_BLE), "tracking still true after cross-API double-free");
}

// Phase 6: btMarkMemReleased() as a pre-release sentinel.
// Marks BLE memory as released without calling any real ESP-IDF function
// (simulates a release that occurred outside the wrap, e.g. via a ROM call).
// Every subsequent wrap call must treat the memory as already gone and return
// ESP_OK without performing a real release.
static void phase_6() {
  Serial.println("[BT_MEM_WRAP] Phase 6: btMarkMemReleased sentinel");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");

  // Flag the memory as released without going through ESP-IDF.
  btMarkMemReleased(BT_MODE_BLE);
  check(btMemReleased(BT_MODE_BLE), "btMarkMemReleased updates tracking flag");

  // All three release paths must now be no-ops.
  esp_err_t ret = esp_bt_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "esp_bt_mem_release after btMarkMemReleased is ESP_OK (no real call)");

  ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  check(ret == ESP_OK, "esp_bt_controller_mem_release after btMarkMemReleased is ESP_OK (no real call)");

  check(btMemRelease(BT_MODE_BLE), "btMemRelease after btMarkMemReleased is true (no-op)");
  check(btMemReleased(BT_MODE_BLE), "tracking still true after all no-op calls");
}

// Phase 7: btStartMode(BLE) must fail after btMemRelease(BLE).
// Once BLE controller memory has been freed it cannot be reclaimed for BLE.
// btStart() uses the default BT_MODE (often BTDM on ESP32) and may downgrade
// to Classic-only when only BLE memory was released — test the specific mode.
static void phase_7() {
  Serial.println("[BT_MEM_WRAP] Phase 7: btStartMode(BLE) fails after btMemRelease(BLE)");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");
  check(!btStarted(), "BT not started at boot");

  check(btMemRelease(BT_MODE_BLE), "btMemRelease(BLE) succeeds");
  check(btMemReleased(BT_MODE_BLE), "tracking updated");

  // BLE memory is gone — BLE mode cannot be initialized.
  check(!btStartMode(BT_MODE_BLE), "btStartMode(BLE) returns false after btMemRelease(BLE)");
  check(!btStarted(), "btStarted() false after failed btStartMode(BLE)");
}

// Phase 8: full lifecycle — start BT, attempt release while running (must be
// rejected without corrupting tracking state), stop BT, then release memory.
//
// This exercises two important invariants:
//   a) btMemRelease() must not update tracking when the underlying ESP-IDF call
//      fails (i.e. while the controller is active).
//   b) After a clean btStop(), btMemRelease() must succeed.
static void phase_8() {
  Serial.println("[BT_MEM_WRAP] Phase 8: start, release-while-running rejected, stop, release");

  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");
  check(!btStarted(), "BT not started at boot");

  // Start BT while memory is still available.
  check(btStart(), "btStart() succeeds before any memory release");
  check(btStarted(), "btStarted() true after btStart");

#if defined(CONFIG_BT_DUAL_MODE_ARCH)
  // Split-controller targets: a partial release is a logical contract, so it
  // is allowed while running and must not stop or narrow the controller.
  check(btMemRelease(BT_MODE_BLE), "BTDM: release while running is logical-only");
  check(btMemReleased(BT_MODE_BLE), "BTDM: contract set while running");
  check(btStarted(), "BTDM: controller still running after logical release");
#else
  // Attempt to release memory while the controller is still active.
  // ESP-IDF will reject the request; btMemRelease() must return false and
  // must NOT mark the memory as released.
  check(!btMemRelease(BT_MODE_BLE), "btMemRelease while BT is running returns false");
  check(!btMemReleased(BT_MODE_BLE), "tracking NOT updated after rejected release");
#endif

  // Stop BT cleanly (disable + deinit → controller returns to IDLE state).
  check(btStop(), "btStop() succeeds");
  check(!btStarted(), "btStarted() false after btStop");

  // Memory release must now succeed because the controller is idle.
  check(btMemRelease(BT_MODE_BLE), "btMemRelease succeeds after btStop");
  check(btMemReleased(BT_MODE_BLE), "tracking updated after successful release");
}

// Phase 9: Matter-style release — simulate BLEManagerImpl::InitESPBleLayer().
//
// The Matter stack (connectedhomeip BLEManagerImpl on ESP32) calls
// esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT) during its BLE
// initialization to free the unused Classic BT controller memory before
// starting BLE commissioning.
//
// On chips without Classic BT this code path is not taken; on ESP32 the
// __wrap_esp_bt_controller_mem_release intercept must:
//   a) update the Classic BT tracking flag so btMemReleased(BT_MODE_CLASSIC_BT)
//      returns true, and
//   b) NOT touch the BLE tracking flag, leaving BLE fully usable for
//      commissioning via btStartMode(BT_MODE_BLE).
//
// This test verifies both invariants and then confirms that a subsequent
// btStartMode(BT_MODE_BLE) succeeds — proving that the --wrap intercept does
// not crash or corrupt state when called from the Matter stack.
//
// On split-controller (S31) BTDM builds this direct Classic release + BLE-only
// start only becomes valid once the controller can select a single half at
// runtime. Until then the implementation must refuse it (or this phase must be
// skipped for CONFIG_BT_DUAL_MODE_ARCH); the Arduino btMemRelease() does not
// make that call in a BTDM build. See DESIGN.md, "To reclaim the unused half
// in a BTDM build".
#if CONFIG_BT_CLASSIC_ENABLED
static void phase_9() {
  Serial.println("[BT_MEM_WRAP] Phase 9: Matter-style Classic BT release (BLEManagerImpl simulation)");

  check(!btMemReleased(BT_MODE_CLASSIC_BT), "Classic BT mem not released at boot");
  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released at boot");

  // Simulate the exact call that Matter's BLEManagerImpl makes on ESP32.
  esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  check(ret == ESP_OK, "Matter-style esp_bt_controller_mem_release(CLASSIC) returns ESP_OK");
  check(btMemReleased(BT_MODE_CLASSIC_BT), "Classic BT tracking updated after Matter-style release");
  check(!btMemReleased(BT_MODE_BLE), "BLE tracking NOT affected by Classic BT release");

  // BLE must still be usable — Matter uses it for commissioning.
  check(btStartMode(BT_MODE_BLE), "btStartMode(BLE) succeeds after Classic BT released");
  check(btStarted(), "btStarted() true after BLE start");
  check(btStop(), "btStop() succeeds");
  check(!btStarted(), "btStarted() false after btStop");
}
#else
static void phase_9() {
  // Classic BT is not supported on this chip, so Matter will not call
  // esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT).  Nothing to test.
  Serial.println("[BT_MEM_WRAP] Phase 9: SKIP (Classic BT not supported on this target)");
}
#endif

// Phase 10: the shared Bluedroid host manager.
//
// Bluedroid is a single host. The core reference-counts it for BLE and
// BluetoothSerial: the first user starts it, the last one stops it, and an
// externally enabled host is adopted and left up. This phase exercises the
// manager directly, then the memory policy it must preserve when
// esp_bt_mem_release() is implemented on the split-controller targets.
//
// When the S31 controller gains runtime mode selection, flip the BTDM branch
// below to the physical path and assert the unused half is freed while the
// surviving half still starts. See DESIGN.md, "To reclaim the unused half in a
// BTDM build".
#if defined(CONFIG_BLUEDROID_ENABLED)
static void phase_10() {
  Serial.println("[BT_MEM_WRAP] Phase 10: shared Bluedroid host manager");

  check(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED, "host uninitialized at boot");
  check(!btStarted(), "controller idle at boot");

  // First user starts the controller and host.
  check(btBluedroidHostAcquire(), "first acquire starts the host");
  check(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED, "host enabled after first acquire");
  check(btStarted(), "controller enabled after first acquire");

  // Second user adopts the running host; the first release must leave it up.
  check(btBluedroidHostAcquire(), "second acquire adopts the running host");
  check(btBluedroidHostRelease() == BT_HOST_STILL_IN_USE, "first release leaves the host in use");
  check(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED, "host stays enabled while shared");

  // Last user stops the host; the controller stop is deferred to finalize.
  check(btBluedroidHostRelease() == BT_HOST_STOPPED, "last release stops the host");
  check(esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED, "host disabled after last release");
  btBluedroidHostFinalize();
  check(!btStarted(), "controller stopped after finalize");

  // External owner: the host is already enabled outside the manager. Acquire
  // adopts it; release must leave it running for that owner.
  check(btStart(), "external btStart succeeds");
  check(esp_bluedroid_init() == ESP_OK, "external host init succeeds");
  check(esp_bluedroid_enable() == ESP_OK, "external host enable succeeds");
  check(btBluedroidHostAcquire(), "acquire adopts the external host");
  check(btBluedroidHostRelease() == BT_HOST_EXTERNAL, "release leaves the external host up");
  check(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED, "external host still enabled");
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  btStop();
  check(!btStarted(), "external host cleaned up");

  // Memory policy: on a BTDM split-controller build a partial release is a
  // logical contract only; it must not narrow or free the controller. A
  // single-half build keeps the physical path, which will free the half when
  // the controller implements esp_bt_mem_release().
  check(!btMemReleased(BT_MODE_BLE), "BLE mem not released before policy check");
#if defined(CONFIG_BT_DUAL_MODE_ARCH)
  check(btMemRelease(BT_MODE_BLE), "BTDM: BLE release recorded");
  check(btMemReleased(BT_MODE_BLE), "BTDM: BLE contract set");
  check(btStart(), "BTDM: controller still starts after logical release");
  check(btStop(), "BTDM: controller stops after logical release");
#else
  check(btMemRelease(BT_MODE_BLE), "single-half: BLE release succeeds");
  check(btMemReleased(BT_MODE_BLE), "single-half: BLE contract set");
#endif
}
#else
static void phase_10() {
  Serial.println("[BT_MEM_WRAP] Phase 10: SKIP (Bluedroid host manager not available on this host)");
}
#endif

static void run_phase(int phase) {
  pass_count = 0;
  fail_count = 0;

  switch (phase) {
    case 1:  phase_1(); break;
    case 2:  phase_2(); break;
    case 3:  phase_3(); break;
    case 4:  phase_4(); break;
    case 5:  phase_5(); break;
    case 6:  phase_6(); break;
    case 7:  phase_7(); break;
    case 8:  phase_8(); break;
    case 9:  phase_9(); break;
    case 10: phase_10(); break;
    default: Serial.printf("[BT_MEM_WRAP] ERROR: unknown phase %d\n", phase); return;
  }

  Serial.printf("[BT_MEM_WRAP] Phase %d: %d passed, %d failed\n", phase, pass_count, fail_count);
  Serial.printf("[BT_MEM_WRAP] Phase %d: %s\n", phase, (fail_count == 0) ? "PASSED" : "FAILED");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[BT_MEM_WRAP] Ready");
  Serial.println("[BT_MEM_WRAP] Send phase:");

  // Wait for "PHASE N" from the test harness.
  String cmd = "";
  while (cmd.length() == 0) {
    if (Serial.available()) {
      cmd = Serial.readStringUntil('\n');
      cmd.trim();
    }
    delay(10);
  }

  Serial.printf("[BT_MEM_WRAP] Received: %s\n", cmd.c_str());

  if (cmd.startsWith("PHASE ")) {
    int phase = cmd.substring(6).toInt();
    run_phase(phase);
    // Restart after every phase so the next phase starts with a clean BT
    // memory state.  This avoids relying on dut.hard_reset() which is not
    // reliably available in all test environments.
    Serial.flush();
    ESP.restart();
  } else {
    Serial.printf("[BT_MEM_WRAP] ERROR: unexpected command: %s\n", cmd.c_str());
  }
}

void loop() {}

#else  // BT controller not available

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("[BT_MEM_WRAP] SKIP: BT controller not available");
}

void loop() {}

#endif
