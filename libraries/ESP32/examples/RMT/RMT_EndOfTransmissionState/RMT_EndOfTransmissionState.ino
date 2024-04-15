#define BLINK_GPIO 2
#define EOT_INITIAL_STATE_TIME_MS 1000

// BLINK_GPIO shall start at RMT_EOT (HIGH or LOW) as initial state for EOT_INITIAL_STATE_TIME_MS,
// BLINK: 1 second ON, 1 second OFF and then return/stay to RMT_EOT level at the end.
#define RMT_EOT HIGH

// RMT is at 400KHz with a 2.5us tick
// This RMT data sends a 0.5Hz pulse with 1s High and 1s Low signal
rmt_data_t blink_1s_rmt_data[] = {
  // 400,000 x 2.5us = 1 second ON
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  // 400,000 x 2.5us = 1 second OFF
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  // Looping mode needs a Zero ending data to mark the EOF
  { 0, 0, 0, 0 }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Blink testing...");
  Serial.flush();

  // 1 RMT Block has 64 RMT_SYMBOLS (ESP32|ESP32S2) or 48 RMT_SYMBOLS (ESP32C3|ESP32S3)
  if (!rmtInit(BLINK_GPIO, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 400000)) {  //2.5us tick
    Serial.println("===> rmtInit Error!");
  }

  // sets the End of Transmission Level to HIGH, after writing to the pin. DEFAULT is LOW.
  rmtSetEOT(BLINK_GPIO, RMT_EOT);
  // set initial RMT state by writing a single RMT data
  rmt_data_t initStateSetup_rmt_data[] = { { 1, RMT_EOT, 0, 0 } };
  rmtWrite(BLINK_GPIO, initStateSetup_rmt_data, RMT_SYMBOLS_OF(initStateSetup_rmt_data), RMT_WAIT_FOR_EVER);
  Serial.printf("\nLED GPIO%d start in the initial level %s\n", BLINK_GPIO, RMT_EOT == LOW ? "LOW" : "HIGH");
  delay(EOT_INITIAL_STATE_TIME_MS);  // set initial state of the LED is set by RMT_EOT.

  // Send the data and wait until it is done - set EOT level to HIGH
  Serial.printf("\nLED GPIO%d Blinks 1 second HIGH - 1 second LOW.\n", BLINK_GPIO);
  if (!rmtWrite(BLINK_GPIO, blink_1s_rmt_data, RMT_SYMBOLS_OF(blink_1s_rmt_data) - 2, RMT_WAIT_FOR_EVER)) {
    Serial.println("===> rmtWrite Blink 1s Error!");
  }
  Serial.printf("\nLED GPIO%d goes to the EOT level %s\n", BLINK_GPIO, RMT_EOT == LOW ? "LOW" : "HIGH");
}

void loop() {}
