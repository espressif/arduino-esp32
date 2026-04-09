// =============================================================================
// ESP32 HFP HCI Audio — example sketch
//
// Demonstrates bidirectional SCO audio over HCI on Arduino ESP32, using the
// ring-buffered pull-callback architecture required by the Bluedroid stack.
//
// WHAT THIS EXAMPLE SHOWS
// -----------------------
// The ESP32 acts as a Bluetooth Hands-Free Profile (HFP) client.  When a call
// is active the stack opens a SCO audio channel (CVSD 8 kHz or mSBC 16 kHz).
// With CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y the PCM samples are routed to the
// application layer instead of an on-chip codec, giving full software control.
//
// Two audio modes are selectable at compile time via AUDIO_MODE below:
//
//   AUDIO_MODE_TONE
//     A 1 kHz sine wave is generated locally and sent to the phone.
//     Tests the TX (outgoing) path only.
//     The incoming audio from the phone is discarded.
//
//   AUDIO_MODE_LOOPBACK
//     Audio received from the phone is echoed straight back.
//     Tests both RX (incoming) and TX (outgoing) paths end-to-end.
//     The caller hears their own voice with a small delay.
//
// AUDIO ARCHITECTURE
// ------------------
// The Bluedroid BTA task drives SCO by calling the registered outgoing-data
// callback (hfOutgoingDataCb) in a tight loop.  The loop runs only after the
// application calls esp_hf_client_outgoing_data_ready() — the stack will NOT
// pull data on its own.
//
// To decouple the producer (tone generator / loopback receiver) from the BTA
// consumer and absorb scheduling jitter, a FreeRTOS StreamBuffer is used as a
// ring buffer.  The producer writes frames into it; the callback drains it.
// On underrun the callback returns 0, which tells the stack to skip that SCO
// slot cleanly rather than sending silence or crashing.
//
//   TONE mode:
//     esp_timer (exact frame period)
//       → generateToneSamples() → StreamBuffer    (producer)
//       → esp_hf_client_outgoing_data_ready()     (triggers BTA pull)
//     hfOutgoingDataCb (BTA task)
//       → xStreamBufferReceive()                  (consumer)
//
//   LOOPBACK mode:
//     hfIncomingDataCb (BTA task)
//       → xStreamBufferSend() into StreamBuffer   (producer)
//     esp_timer (exact frame period)
//       → esp_hf_client_outgoing_data_ready()     (triggers BTA pull)
//     hfOutgoingDataCb (BTA task)
//       → xStreamBufferReceive()                  (consumer)
//
// FOR I2S INTEGRATION (ADC / DAC)
// -------------------
// Replace the esp_timer producer with an I2S DMA callback that writes
// captured ADC samples to the StreamBuffer, then calls
// esp_hf_client_outgoing_data_ready().
// For playback, add a second StreamBuffer filled by hfIncomingDataCb and
// drained by an I2S DAC task.  Configure the I2S sample rate in the
// AUDIO_STATE_EVT handler based on the is_msbc flag (8 kHz vs 16 kHz).
//
// REQUIRED BUILD CONFIG (defconfig.esp32 / sdkconfig)
// ----------------------------------------------------
//   CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y
//   CONFIG_BTDM_CTRL_BR_EDR_SCO_DATA_PATH_HCI=y
// =============================================================================

// -----------------------------------------------------------------------------
// Compile-time mode selection
// -----------------------------------------------------------------------------
#include <Arduino.h>
#define AUDIO_MODE_TONE     1  // generate a local 1 kHz sine → phone
#define AUDIO_MODE_LOOPBACK 2  // echo phone audio back to the phone

#define AUDIO_MODE AUDIO_MODE_LOOPBACK  // <-- change to AUDIO_MODE_TONE to test TX only

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include "esp32-hal-bt-mem.h"
#include "esp32-hal-bt.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_hf_client_legacy_api.h"  // esp_hf_client_outgoing_data_ready()
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

namespace {

// -----------------------------------------------------------------------------
// Tone parameters (AUDIO_MODE_TONE only)
// -----------------------------------------------------------------------------
constexpr float kToneFrequencyHz = 1000.0f;
constexpr int16_t kToneAmplitude = 10000;  // ~30% of full scale

// -----------------------------------------------------------------------------
// Codec sample rates
// -----------------------------------------------------------------------------
constexpr float kCvsdRateHz = 8000.0f;   // CVSD:  8 kHz, 30 samples / 3750 µs
constexpr float kMsbcRateHz = 16000.0f;  // mSBC: 16 kHz, 120 samples / 7500 µs

// -----------------------------------------------------------------------------
// BT / reconnect tuning
// -----------------------------------------------------------------------------
constexpr uint8_t kMaxBondedDevices = 8;
constexpr uint32_t kReconnectIntervalMs = 5000;
constexpr uint32_t kAudioConnectRetryMs = 3000;

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------
bool slc_connected = false;     // Service Level Connection (HFP control channel) established
bool audio_connected = false;   // SCO audio channel open and ready
bool audio_connecting = false;  // SCO connection attempt in progress
bool call_active = false;       // phone reports a call in progress (CIND call status)
bool have_remote_bda = false;   // we know the address of the phone to connect to
bool hf_ready = false;          // HFP profile stack initialized successfully
bool is_msbc = false;           // true = mSBC 16 kHz codec; false = CVSD 8 kHz

esp_bd_addr_t remote_bda = {0};  // Bluetooth address of the remote phone

uint32_t last_hf_connect_attempt_ms = 0;     // timestamp of last HFP connect attempt (for rate limiting)
uint32_t last_audio_connect_attempt_ms = 0;  // timestamp of last SCO connect attempt (for rate limiting)
uint32_t last_audio_debug_ms = 0;            // timestamp of last diagnostic Serial print

// -----------------------------------------------------------------------------
// Oscillator state — only ever written inside audioTimerCb (esp_timer task).
// No lock needed because the timer task is the sole writer.
// -----------------------------------------------------------------------------
float phase = 0.0f;  // current sine wave phase in radians [0, 2π)

// -----------------------------------------------------------------------------
// Diagnostics counters — written from the BTA task, read from loop().
// Snapshot with noInterrupts()/interrupts() before reading in loop().
// -----------------------------------------------------------------------------
volatile uint32_t outgoing_cb_count = 0;     // total number of times hfOutgoingDataCb was called
volatile uint32_t outgoing_cb_bytes = 0;     // total PCM bytes successfully delivered to the stack
volatile uint32_t outgoing_cb_last_len = 0;  // frame size the stack last requested (60 or 240 bytes)
volatile uint32_t outgoing_cb_underrun = 0;  // times the ring buffer was empty when the callback fired

// -----------------------------------------------------------------------------
// Audio ring buffer + periodic trigger timer
//
// CVSD:  chunk = 60 bytes  (30 samples × 2),  interval = 3750 µs
// mSBC:  chunk = 240 bytes (120 samples × 2), interval = 7500 µs
//
// kRingBufFrames deep buffer absorbs BTA task scheduling jitter: delayed
// BTA wakeups cause frames to queue rather than being dropped.
// -----------------------------------------------------------------------------
constexpr uint32_t kCvsdIntervalUs = 3750;
constexpr uint32_t kMsbcIntervalUs = 7500;
constexpr size_t kRingBufFrames = 8;  // ~60 ms headroom at mSBC rate

StreamBufferHandle_t audio_ring = nullptr;  // FreeRTOS StreamBuffer used as the audio ring buffer
esp_timer_handle_t audio_timer = nullptr;   // high-resolution periodic timer that drives the SCO pull cycle

// -----------------------------------------------------------------------------
// generateToneSamples
//
// Fills buf with sample_count signed 16-bit PCM samples of a sine wave at
// kToneFrequencyHz.  Uses and advances the global phase accumulator so that
// successive calls produce a continuous, phase-coherent waveform.
// Only compiled in AUDIO_MODE_TONE.
// -----------------------------------------------------------------------------
#if AUDIO_MODE == AUDIO_MODE_TONE
void generateToneSamples(int16_t *buf, uint16_t sample_count, float sample_rate_hz) {
  const float phase_inc = (2.0f * PI * kToneFrequencyHz) / sample_rate_hz;  // radians to advance per sample
  for (uint16_t i = 0; i < sample_count; ++i) {
    buf[i] = static_cast<int16_t>(kToneAmplitude * sinf(phase));  // scale sine [-1,1] to 16-bit PCM range
    phase += phase_inc;
    if (phase >= 2.0f * PI) {
      phase -= 2.0f * PI;  // wrap phase to [0, 2π) to prevent float precision drift over time
    }
  }
}
#endif

// -----------------------------------------------------------------------------
// audioTimerCb
//
// Periodic timer callback, fired once per SCO frame period (3750 µs for CVSD,
// 7500 µs for mSBC).  Acts as the audio producer and SCO pull trigger.
//
// In TONE mode: generates one frame of sine-wave PCM and writes it into the
// ring buffer before triggering the pull.
//
// In LOOPBACK mode: the ring buffer is already being filled by hfIncomingDataCb,
// so this callback only needs to trigger the pull — no sample generation needed.
//
// Calling esp_hf_client_outgoing_data_ready() posts an event to the BTA task,
// which then calls hfOutgoingDataCb in a loop until the SCO transmit queue is
// full or the callback returns 0.
// -----------------------------------------------------------------------------
void audioTimerCb(void *) {
#if AUDIO_MODE == AUDIO_MODE_TONE
  if (!audio_ring) {
    return;
  }
  const uint16_t samples = is_msbc ? 120 : 30;  // number of PCM samples per SCO frame
  int16_t buf[120];                             // stack-allocated, sized for the largest frame (mSBC)
  generateToneSamples(buf, samples, is_msbc ? kMsbcRateHz : kCvsdRateHz);
  const size_t frame_bytes = samples * sizeof(int16_t);
  // All-or-nothing write: only send if the full frame fits, to prevent partial
  // writes that would break frame alignment for the consumer.
  if (xStreamBufferSpacesAvailable(audio_ring) >= frame_bytes) {
    xStreamBufferSend(audio_ring, buf, frame_bytes, 0);
  }
#endif
  esp_hf_client_outgoing_data_ready();  // signal the BTA task to start pulling audio from hfOutgoingDataCb
}

// -----------------------------------------------------------------------------
// startAudioTimer
//
// Creates the audio ring buffer and starts the periodic SCO trigger timer.
// Must be called once the SCO audio channel is confirmed open (AUDIO_STATE_EVT).
// The ring buffer size and timer period are set according to the active codec.
// Safe to call repeatedly — returns immediately if the timer already exists.
// -----------------------------------------------------------------------------
void startAudioTimer(bool msbc) {
  if (audio_timer) {
    return;  // already running; nothing to do
  }

  const size_t chunk_bytes = msbc ? 240 : 60;  // bytes per SCO frame for the active codec
  // trigger_level=chunk_bytes ensures the consumer only unblocks when a full
  // frame is available, preventing partial reads that would break frame alignment.
  audio_ring = xStreamBufferCreate(chunk_bytes * kRingBufFrames, chunk_bytes);
  if (!audio_ring) {
    Serial.println("xStreamBufferCreate failed");
    return;
  }

  const esp_timer_create_args_t args = {
    .callback = audioTimerCb,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,  // run callback in the dedicated esp_timer task, not an ISR
    .name = "sco_prod",
    .skip_unhandled_events = false,  // queue missed triggers rather than dropping them silently
  };
  esp_err_t err = esp_timer_create(&args, &audio_timer);
  if (err != ESP_OK) {
    Serial.printf("esp_timer_create failed: %s\n", esp_err_to_name(err));
    vStreamBufferDelete(audio_ring);
    audio_ring = nullptr;
    return;
  }
  err = esp_timer_start_periodic(audio_timer, msbc ? kMsbcIntervalUs : kCvsdIntervalUs);
  if (err != ESP_OK) {
    Serial.printf("esp_timer_start_periodic failed: %s\n", esp_err_to_name(err));
    esp_timer_delete(audio_timer);
    audio_timer = nullptr;
    vStreamBufferDelete(audio_ring);
    audio_ring = nullptr;
  }
}

// -----------------------------------------------------------------------------
// stopAudioTimer
//
// Stops and destroys the periodic timer and frees the ring buffer.
// Called when the SCO audio channel closes (AUDIO_STATE_EVT disconnected).
// Safe to call when the timer or ring buffer is already null.
// -----------------------------------------------------------------------------
void stopAudioTimer() {
  if (audio_timer) {
    esp_timer_stop(audio_timer);
    esp_timer_delete(audio_timer);
    audio_timer = nullptr;
  }
  if (audio_ring) {
    vStreamBufferDelete(audio_ring);
    audio_ring = nullptr;
  }
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// printBda — prints a Bluetooth device address in human-readable XX:XX:XX:XX:XX:XX format.
void printBda(const esp_bd_addr_t bda) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

// rememberBda — saves the remote device address so we can reconnect after a drop.
void rememberBda(const esp_bd_addr_t bda) {
  memcpy(remote_bda, bda, ESP_BD_ADDR_LEN);
  have_remote_bda = true;
}

// logEspCall — prints the result of an ESP-IDF API call.  Logs OK or the error name + code.
void logEspCall(const char *what, esp_err_t err) {
  if (err == ESP_OK) {
    Serial.printf("%s: OK\n", what);
    return;
  }
  Serial.printf("%s: %s (0x%04X)\n", what, esp_err_to_name(err), static_cast<unsigned>(err));
}

// hfConnStateName — maps HFP connection state enum to a printable string for diagnostics.
// The connection goes through CONNECTING → RFCOMM_CONNECTED → SLC_CONNECTED before
// the HFP control channel is fully usable.  Audio can only open after SLC_CONNECTED.
const char *hfConnStateName(esp_hf_client_connection_state_t s) {
  switch (s) {
    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED:  return "DISCONNECTED";
    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING:    return "CONNECTING";
    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED:     return "RFCOMM_CONNECTED";
    case ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED: return "SLC_CONNECTED";
    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING: return "DISCONNECTING";
  }
  return "UNKNOWN";
}

// hfAudioStateName — maps HFP audio state enum to a printable string for diagnostics.
// CONNECTED = CVSD codec (8 kHz); CONNECTED_MSBC = mSBC codec (16 kHz, wideband speech).
const char *hfAudioStateName(esp_hf_client_audio_state_t s) {
  switch (s) {
    case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED:   return "DISCONNECTED";
    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTING:     return "CONNECTING";
    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED:      return "CONNECTED_CVSD";
    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC: return "CONNECTED_MSBC";
  }
  return "UNKNOWN";
}

// -----------------------------------------------------------------------------
// hfOutgoingDataCb
//
// Pull callback registered with the HFP stack.  The BTA task calls this
// repeatedly after each esp_hf_client_outgoing_data_ready() trigger, filling
// the SCO transmit queue until it is full (HIGH_WM = 20 slots) or we return 0.
//
// Reads one frame from the ring buffer.  Returns the frame size on success, or
// 0 on underrun — returning 0 tells the stack to stop pulling for this cycle
// and skip the SCO slot cleanly (no silence, no crash).
//
// Expected normal behavior: underrun ≈ cb_count / 2.
//   Each trigger cycle: first call drains one frame (success), second call
//   finds the buffer empty (underrun) and stops the loop.  Underrun
//   significantly above cb_count/2 means the producer is falling behind.
// -----------------------------------------------------------------------------
uint32_t hfOutgoingDataCb(uint8_t *buf, uint32_t len) {
  if (!buf || len == 0 || !audio_ring) {
    return 0;  // stack not ready or audio not started yet
  }

  // Only read if a complete frame is available to avoid partial reads that
  // would break frame alignment and cause audible artifacts.
  if (xStreamBufferBytesAvailable(audio_ring) < len) {
    ++outgoing_cb_underrun;
    return 0;
  }
  const size_t got = xStreamBufferReceive(audio_ring, buf, len, 0);

  ++outgoing_cb_count;
  outgoing_cb_last_len = len;  // record the frame size the stack is requesting
  outgoing_cb_bytes += got;    // accumulate bytes actually delivered

  if (got < len) {
    ++outgoing_cb_underrun;
    return 0;  // signal underrun: stack skips this SCO slot
  }
  return len;  // full frame delivered; stack may call again to fill more queue slots
}

// -----------------------------------------------------------------------------
// hfIncomingDataCb
//
// Push callback registered with the HFP stack.  The BTA task calls this with
// each received SCO frame, already decoded to raw 16-bit PCM — the internal
// Bluedroid codec handles CVSD and mSBC decoding transparently, so the app
// always receives plain PCM regardless of which codec the phone negotiated.
//
// LOOPBACK mode: the received PCM is written straight into the same ring buffer
//   that hfOutgoingDataCb drains, creating a loopback: caller hears themselves.
//   The write is non-blocking; if the ring buffer is full the frame is dropped
//   rather than stalling the BTA task.
//
// TONE mode: incoming audio is discarded — we only test the TX path.
// -----------------------------------------------------------------------------
void hfIncomingDataCb(const uint8_t *buf, uint32_t len) {
#if AUDIO_MODE == AUDIO_MODE_LOOPBACK
  if (!audio_ring || !buf || len == 0) {
    return;
  }
  // All-or-nothing write: only enqueue if the full frame fits, to prevent
  // partial writes that would break frame alignment for the outgoing callback.
  if (xStreamBufferSpacesAvailable(audio_ring) >= len) {
    xStreamBufferSend(audio_ring, buf, len, 0);
  }
#else
  (void)buf;
  (void)len;  // discard in TONE mode — incoming audio not needed
#endif
}

// -----------------------------------------------------------------------------
// tryConnectHfp
//
// Attempts to open an HFP Service Level Connection (SLC) to the remembered
// remote phone.  The SLC is the HFP control channel and must be established
// before any call control or audio channel can be opened.
//
// Guards: does nothing if the stack is not yet ready, if no phone address is
// known, or if already connected.  Rate-limits retries to kReconnectIntervalMs
// to avoid flooding the stack with connection attempts.
// -----------------------------------------------------------------------------
void tryConnectHfp(const char *reason) {
  if (!hf_ready || !have_remote_bda || slc_connected) {  // nothing to do if not ready, no peer known, or already connected
    return;
  }

  const uint32_t now_ms = millis();
  if (now_ms - last_hf_connect_attempt_ms < kReconnectIntervalMs) {  // rate-limit retries
    return;
  }

  last_hf_connect_attempt_ms = now_ms;
  Serial.printf("Connecting HFP (%s) to ", reason);
  printBda(remote_bda);
  Serial.println();
  logEspCall("esp_hf_client_connect", esp_hf_client_connect(remote_bda));
}

// -----------------------------------------------------------------------------
// tryConnectAudio
//
// Attempts to open the SCO audio channel to the remote phone.  This upgrades
// an established HFP SLC to a full audio connection so that PCM data can flow.
//
// Guards: requires a known peer address, an active SLC, and an active call
// (the phone will reject SCO if there is no call in progress).  Also guards
// against opening a second channel if already connected.  If a connection
// attempt is already in flight, rate-limits retries to kAudioConnectRetryMs.
// -----------------------------------------------------------------------------
void tryConnectAudio(const char *reason) {
  if (!have_remote_bda || !slc_connected || !call_active || audio_connected) {  // SCO requires an active SLC and an in-progress call
    return;
  }

  const uint32_t now_ms = millis();
  if (audio_connecting && (now_ms - last_audio_connect_attempt_ms) < kAudioConnectRetryMs) {  // already attempting; wait before retrying
    return;
  }

  last_audio_connect_attempt_ms = now_ms;
  audio_connecting = true;
  Serial.printf("Opening SCO audio (%s)\n", reason);
  logEspCall("esp_hf_client_connect_audio", esp_hf_client_connect_audio(remote_bda));
}

// -----------------------------------------------------------------------------
// connectFirstBondedDevice
//
// Retrieves the list of previously paired phones from the Bluedroid bond store
// and initiates an HFP connection to the most recently bonded one.
// Called once the HFP profile stack reports it is ready (PROF_STATE_EVT).
// If no bonded device is found, prints a message asking the user to pair first.
// -----------------------------------------------------------------------------
void connectFirstBondedDevice() {
  int bonded_count = esp_bt_gap_get_bond_device_num();  // query how many phones are in the bond store
  if (bonded_count <= 0) {
    Serial.println("No bonded phone yet. Pair from the phone's Bluetooth menu.");
    return;
  }
  if (bonded_count > kMaxBondedDevices) {
    bonded_count = kMaxBondedDevices;  // cap to our stack-allocated array size
  }

  esp_bd_addr_t bonded_devices[kMaxBondedDevices];
  int device_count = bonded_count;
  esp_err_t err = esp_bt_gap_get_bond_device_list(&device_count, bonded_devices);  // retrieve the actual addresses
  if (err != ESP_OK || device_count <= 0) {
    logEspCall("esp_bt_gap_get_bond_device_list", err);
    return;
  }

  rememberBda(bonded_devices[0]);  // use the first (most recently bonded) phone
  Serial.print("Found bonded phone ");
  printBda(remote_bda);
  Serial.println();
  tryConnectHfp("bonded device");
}

// -----------------------------------------------------------------------------
// gapCallback
//
// Generic Access Profile (GAP) event callback, handling Bluetooth pairing events.
//
// AUTH_CMPL_EVT: fires when a pairing attempt completes (success or failure).
//   On success, saves the peer address and immediately tries to open the HFP
//   SLC so the phone is ready for calls without user intervention.
//
// CFM_REQ_EVT: fires during Secure Simple Pairing (SSP) numeric comparison.
//   The phone displays a 6-digit number; normally the user confirms it matches.
//   Here we auto-accept — sufficient for a trusted development device.
//
// PIN_REQ_EVT: fires when pairing with an older phone that uses legacy PIN mode.
//   Responds with PIN "0000".  Some phones require this path instead of SSP.
// -----------------------------------------------------------------------------
void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        rememberBda(param->auth_cmpl.bda);
        Serial.print("Pairing complete with ");
        printBda(param->auth_cmpl.bda);
        Serial.printf(" (%s)\n", param->auth_cmpl.device_name);
        tryConnectHfp("pairing complete");  // immediately try to open the HFP control channel
      } else {
        Serial.printf("Pairing failed: status=%d\n", param->auth_cmpl.stat);
      }
      break;

    case ESP_BT_GAP_CFM_REQ_EVT:
      rememberBda(param->cfm_req.bda);
      Serial.printf("SSP confirm %06lu for ", param->cfm_req.num_val);  // log the numeric code for reference
      printBda(param->cfm_req.bda);
      Serial.println();
      logEspCall(
        "esp_bt_gap_ssp_confirm_reply", esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true)
      );  // auto-accept: skip user confirmation for this example
      break;

    case ESP_BT_GAP_PIN_REQ_EVT:
    {
      esp_bt_pin_code_t pin_code = {'0', '0', '0', '0'};  // default legacy PIN
      rememberBda(param->pin_req.bda);
      Serial.print("Legacy PIN requested for ");
      printBda(param->pin_req.bda);
      Serial.println();
      logEspCall("esp_bt_gap_pin_reply", esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code));  // respond with PIN "0000"
      break;
    }

    default: break;
  }
}

// -----------------------------------------------------------------------------
// hfClientEventCb
//
// HFP client profile event callback.  Handles the full lifecycle of an HFP
// connection: profile initialization, SLC connect/disconnect, SCO audio
// channel open/close, and call status changes.
//
// PROF_STATE_EVT: the HFP stack has finished initializing (or was already
//   initialized).  Safe to start using the HFP API from this point.
//
// CONNECTION_STATE_EVT: SLC connection state changed.  SLC_CONNECTED means
//   the HFP control channel is fully established and call control commands
//   can be sent.  On disconnect all derived state is cleared.
//
// AUDIO_STATE_EVT: SCO audio channel opened or closed.  On open, the codec
//   type (CVSD or mSBC) is known, so the ring buffer and timer are started
//   with the correct frame size and period.  On close they are torn down.
//
// CIND_CALL_SETUP_EVT: phone is reporting an incoming or outgoing call being
//   set up.  On incoming, we auto-answer and request the SCO channel.
//
// CIND_CALL_EVT: phone reports whether a call is currently active.  We use
//   this as a second trigger to open the SCO channel (some phones send this
//   without a prior CALL_SETUP event).
//
// RING_IND_EVT: phone is ringing.  Logged only; call answering is done in
//   CALL_SETUP.
// -----------------------------------------------------------------------------
void hfClientEventCb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
  switch (event) {
    case ESP_HF_CLIENT_PROF_STATE_EVT:
      // Stack is ready (or was already initialized); safe to use HFP API now.
      hf_ready = (param->prof_stat.state == ESP_HF_INIT_SUCCESS || param->prof_stat.state == ESP_HF_INIT_ALREADY);
      Serial.printf("HFP profile state: %d\n", param->prof_stat.state);
      if (hf_ready) {
        connectFirstBondedDevice();  // try to reconnect to a previously paired phone immediately
      }
      break;

    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
      rememberBda(param->conn_stat.remote_bda);  // keep the peer address up to date
      slc_connected = (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED);
      if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
        // Full disconnect: clear all derived state so reconnect logic starts clean.
        audio_connected = false;
        audio_connecting = false;
        call_active = false;
        is_msbc = false;
      }
      Serial.printf(
        "HFP connection state: %s peer_feat=0x%08lX chld=0x%08lX ", hfConnStateName(param->conn_stat.state),
        static_cast<unsigned long>(param->conn_stat.peer_feat), static_cast<unsigned long>(param->conn_stat.chld_feat)
      );
      printBda(param->conn_stat.remote_bda);
      Serial.println();
      if (slc_connected) {
        // Disable phone-side noise reduction — audio processing is done in software here.
        logEspCall("esp_hf_client_send_nrec", esp_hf_client_send_nrec());
        // Set microphone gain to maximum so the phone captures our audio at full level.
        logEspCall("esp_hf_client_volume_update(mic)", esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15));
      }
      break;

    case ESP_HF_CLIENT_AUDIO_STATE_EVT:
    {
      const auto state = param->audio_stat.state;
      // Update audio state flags based on the new state.
      audio_connecting = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTING);
      audio_connected = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED || state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
      is_msbc = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);

      if (audio_connected) {
        audio_connecting = false;

        // Reset per-session diagnostics so the Serial log is clean for this call.
        outgoing_cb_count = 0;
        outgoing_cb_bytes = 0;
        outgoing_cb_last_len = 0;
        outgoing_cb_underrun = 0;
        last_audio_debug_ms = millis();
        phase = 0.0f;  // restart tone phase so the sine begins cleanly

        // Bump mic gain again now that SCO is open — some phones reset it on audio connect.
        logEspCall("esp_hf_client_volume_update(mic)", esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15));

        Serial.printf(
          "Codec: %s  Mode: %s\n", is_msbc ? "mSBC (16 kHz)" : "CVSD (8 kHz)",
#if AUDIO_MODE == AUDIO_MODE_TONE
          "TONE"
#else
          "LOOPBACK"
#endif
        );
        startAudioTimer(is_msbc);  // create ring buffer and start the periodic SCO pull trigger
      } else if (!audio_connecting) {
        // SCO channel closed (not just mid-connect): tear down the audio pipeline.
        is_msbc = false;
        stopAudioTimer();
      }

      Serial.printf("Audio state: %s\n", hfAudioStateName(state));
      break;
    }

    case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
      // Phone is setting up a call.  STATUS_INCOMING means the phone is ringing.
      Serial.printf("Call setup state: %d\n", param->call_setup.status);
      if (param->call_setup.status == ESP_HF_CALL_SETUP_STATUS_INCOMING) {
        logEspCall("esp_hf_client_answer_call", esp_hf_client_answer_call());  // auto-answer; remove this line for manual answer
        tryConnectAudio("incoming call setup");                                // request SCO as soon as the call is answered
      }
      break;

    case ESP_HF_CLIENT_CIND_CALL_EVT:
      // Phone reports whether a call is in progress.  Used as a fallback trigger
      // for SCO in case CALL_SETUP was not received (e.g. call already active on connect).
      call_active = (param->call.status == ESP_HF_CALL_STATUS_CALL_IN_PROGRESS);
      Serial.printf("Call active: %s\n", call_active ? "yes" : "no");
      if (call_active) {
        tryConnectAudio("call became active");
      } else {
        audio_connecting = false;  // call ended; cancel any pending SCO attempt
      }
      break;

    case ESP_HF_CLIENT_RING_IND_EVT:
      // Phone is ringing — the call has not been answered yet.
      Serial.println("Ring indication");
      break;

    case ESP_HF_CLIENT_AT_RESPONSE_EVT:
      // Response to an AT command we sent (e.g. AT+NREC, AT+VGM).
      Serial.printf("AT response: code=%d cme=%d\n", param->at_response.code, param->at_response.cme);
      break;

    default: break;
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// setup
//
// One-time initialization: starts the Bluetooth controller and Bluedroid stack,
// registers GAP and HFP callbacks, configures pairing, device identity and
// Class of Device, then initializes the HFP client profile.
//
// Critical ordering: esp_bredr_sco_datapath_set() and
// esp_hf_client_register_data_callback() MUST be called before
// esp_hf_client_init() — the stack reads these settings during initialization.
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);  // wait for the Serial monitor to attach before printing anything
  Serial.println();
  Serial.printf(
    "ESP32 HFP HCI Audio example — mode: %s\n",
#if AUDIO_MODE == AUDIO_MODE_TONE
    "TONE (1 kHz sine → phone)"
#else
    "LOOPBACK (echo phone audio back)"
#endif
  );

  // Start the Classic Bluetooth controller (disables BLE to save memory).
  if (!btStarted()) {
    Serial.println("Starting Classic BT controller");
    if (!btStartMode(BT_MODE_CLASSIC_BT)) {
      Serial.println("btStartMode failed");
    }
  } else {
    Serial.println("BT controller already enabled");
  }

  // Initialize and enable the Bluedroid host stack (two-step: init allocates
  // memory, enable starts the stack tasks).
  esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
  Serial.printf("Bluedroid status before init: %d\n", bt_state);
  if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    logEspCall("esp_bluedroid_init", esp_bluedroid_init());
  }

  bt_state = esp_bluedroid_get_status();
  if (bt_state != ESP_BLUEDROID_STATUS_ENABLED) {
    logEspCall("esp_bluedroid_enable", esp_bluedroid_enable());
  }

  // Register the GAP callback to receive pairing events.
  logEspCall("esp_bt_gap_register_callback", esp_bt_gap_register_callback(gapCallback));

  // IO capability NONE → "just works" SSP pairing: no PIN or numeric confirmation
  // required on either side.  Suitable for a headless embedded device.
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
  logEspCall("esp_bt_gap_set_security_param", esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap)));

  // Enable variable PIN for legacy (pre-SSP) phones; actual PIN is handled in gapCallback.
  esp_bt_pin_code_t unused_pin_code = {0};
  logEspCall("esp_bt_gap_set_pin", esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, unused_pin_code));

  // Set the device name shown to the user during pairing.
  logEspCall("esp_bt_dev_set_device_name", esp_bt_dev_set_device_name("ESP32_HFP_AUDIO"));

  // Set the Class of Device so the phone identifies us as a hands-free audio device.
  esp_bt_cod_t cod = {};
  cod.major = ESP_BT_COD_MAJOR_DEV_AV;                              // Audio/Video major class
  cod.minor = 0x08;                                                 // Hands-free minor subclass
  cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY;  // advertise audio + telephony services
  logEspCall("esp_bt_gap_set_cod", esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL));

  // Make the device visible and connectable so the phone can find and pair with it.
  logEspCall("esp_bt_gap_set_scan_mode", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));

  // Route SCO audio over HCI to the application layer (requires the build config flags).
  // This MUST be called before esp_hf_client_init().
  logEspCall("esp_bredr_sco_datapath_set", esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI));

  // Register the audio data callbacks.  These MUST be registered before esp_hf_client_init().
  logEspCall("esp_hf_client_register_data_callback", esp_hf_client_register_data_callback(hfIncomingDataCb, hfOutgoingDataCb));

  // Register the HFP event callback then start the HFP client profile.
  logEspCall("esp_hf_client_register_callback", esp_hf_client_register_callback(hfClientEventCb));
  logEspCall("esp_hf_client_init", esp_hf_client_init());

  Serial.println("Ready. Pair the phone, then place or receive a call.");
}

// -----------------------------------------------------------------------------
// loop
//
// Runs the background reconnect logic and prints a periodic audio diagnostic.
//
// tryConnectHfp / tryConnectAudio are both called here so that if the SLC or
// SCO drops unexpectedly the sketch automatically reconnects without requiring
// a reset.
//
// The diagnostic snapshot uses noInterrupts()/interrupts() to atomically read
// the volatile counters that are updated from the BTA task, preventing torn
// reads on a 32-bit Xtensa core where 32-bit accesses are not atomic.
// -----------------------------------------------------------------------------
void loop() {
  tryConnectHfp("background retry");    // reconnect HFP SLC if it dropped
  tryConnectAudio("background retry");  // reconnect SCO if a call is active but audio is not open

  if (audio_connected) {
    const uint32_t now_ms = millis();
    if (now_ms - last_audio_debug_ms >= 1000)  // print diagnostics once per second
    {
      // Snapshot volatile counters atomically before reading them.
      noInterrupts();
      const uint32_t cb_count = outgoing_cb_count;
      const uint32_t cb_bytes = outgoing_cb_bytes;
      const uint32_t cb_last_len = outgoing_cb_last_len;
      const uint32_t cb_underrun = outgoing_cb_underrun;
      interrupts();

      // Normal: underrun ≈ cb_count/2 (one trailing underrun per trigger cycle).
      // Underrun significantly above cb_count/2 means the producer is falling behind.
      Serial.printf(
        "Audio [%s]: cb_count=%lu bytes=%lu last_len=%lu underrun=%lu\n", is_msbc ? "mSBC/16kHz" : "CVSD/8kHz", static_cast<unsigned long>(cb_count),
        static_cast<unsigned long>(cb_bytes), static_cast<unsigned long>(cb_last_len), static_cast<unsigned long>(cb_underrun)
      );
      last_audio_debug_ms = now_ms;
    }
  }

  delay(20);  // yield to FreeRTOS so BT stack tasks get CPU time
}
