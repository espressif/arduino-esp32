/* LEDC Gamma Curve Fade Arduino Example

   This example demonstrates gamma curve fading on ESP32 variants that support it.
   Gamma correction makes LED brightness changes appear more gradual and natural
   to human eyes compared to linear fading.

   Two methods are supported:
   1. Using a pre-computed Gamma Look-Up Table (LUT) for better performance
   2. Using mathematical gamma correction with a gamma factor

   Supported chips: ESP32-C6, ESP32-C5, ESP32-H2, ESP32-P4 and future chips with Gamma Fade support

   Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
*/

// use 12 bit precision for LEDC timer
#define LEDC_TIMER_12_BIT 12

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ 5000

// define starting duty, target duty and maximum fade time
#define LEDC_START_DUTY  (0)
#define LEDC_TARGET_DUTY (4095)
#define LEDC_FADE_TIME   (2000)

// gamma factor for mathematical calculation
#define LEDC_GAMMA_FACTOR (2.6)

// use gamma LUT for better performance instead of mathematical calculation (gamma factor)
#define USE_GAMMA_LUT 1

// fade LED pins
const uint8_t ledPinR = 4;
const uint8_t ledPinG = 5;
const uint8_t ledPinB = 6;

uint8_t fade_ended = 0;  // status of LED gamma fade
bool fade_in = true;

#ifdef USE_GAMMA_LUT
// Custom Gamma LUT demonstration with 101 steps (Brightness 0 - 100% gamma correction look up table (gamma = 2.6))
// Y = B ^ 2.6 - Pre-computed LUT to save runtime computation
static const float ledcGammaLUT[101] = {
  0.000000, 0.000006, 0.000038, 0.000110, 0.000232, 0.000414, 0.000666, 0.000994, 0.001406, 0.001910, 0.002512, 0.003218, 0.004035, 0.004969, 0.006025,
  0.007208, 0.008525, 0.009981, 0.011580, 0.013328, 0.015229, 0.017289, 0.019512, 0.021902, 0.024465, 0.027205, 0.030125, 0.033231, 0.036527, 0.040016,
  0.043703, 0.047593, 0.051688, 0.055993, 0.060513, 0.065249, 0.070208, 0.075392, 0.080805, 0.086451, 0.092333, 0.098455, 0.104821, 0.111434, 0.118298,
  0.125416, 0.132792, 0.140428, 0.148329, 0.156498, 0.164938, 0.173653, 0.182645, 0.191919, 0.201476, 0.211321, 0.221457, 0.231886, 0.242612, 0.253639,
  0.264968, 0.276603, 0.288548, 0.300805, 0.313378, 0.326268, 0.339480, 0.353016, 0.366879, 0.381073, 0.395599, 0.410461, 0.425662, 0.441204, 0.457091,
  0.473325, 0.489909, 0.506846, 0.524138, 0.541789, 0.559801, 0.578177, 0.596920, 0.616032, 0.635515, 0.655374, 0.675610, 0.696226, 0.717224, 0.738608,
  0.760380, 0.782542, 0.805097, 0.828048, 0.851398, 0.875148, 0.899301, 0.923861, 0.948829, 0.974208, 1.000000,
};
#endif

void ARDUINO_ISR_ATTR LED_FADE_ISR() {
  fade_ended += 1;
}

void setup() {
  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  // Setup timer with given frequency, resolution and attach it to a led pin with auto-selected channel
  ledcAttach(ledPinR, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinG, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinB, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);

#if USE_GAMMA_LUT  // Use default gamma LUT for better performance
  ledcSetGammaTable(ledcGammaLUT, 101);
#else  // Use mathematical gamma correction (default, more flexible)
  ledcSetGammaFactor(LEDC_GAMMA_FACTOR);  // This is optional to set custom gamma factor (default is 2.8)
#endif

  // Setup and start gamma curve fade on led (duty from 0 to 4095)
  ledcFadeGamma(ledPinR, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME);
  ledcFadeGamma(ledPinG, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME);
  ledcFadeGamma(ledPinB, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME);
  Serial.println("LED Gamma Fade on started.");

  // Wait for fade to end
  delay(LEDC_FADE_TIME);

  // Setup and start gamma curve fade off led and use ISR (duty from 4095 to 0)
  ledcFadeGammaWithInterrupt(ledPinR, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
  ledcFadeGammaWithInterrupt(ledPinG, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
  ledcFadeGammaWithInterrupt(ledPinB, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
  Serial.println("LED Gamma Fade off started.");
}

void loop() {
  // Check if fade_ended flag was set to true in ISR
  if (fade_ended == 3) {
    Serial.println("LED gamma fade ended");
    fade_ended = 0;

    // Check what gamma fade should be started next
    if (fade_in) {
      ledcFadeGammaWithInterrupt(ledPinR, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      ledcFadeGammaWithInterrupt(ledPinG, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      ledcFadeGammaWithInterrupt(ledPinB, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      Serial.println("LED Gamma Fade in started.");
      fade_in = false;
    } else {
      ledcFadeGammaWithInterrupt(ledPinR, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      ledcFadeGammaWithInterrupt(ledPinG, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      ledcFadeGammaWithInterrupt(ledPinB, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      Serial.println("LED Gamma Fade out started.");
      fade_in = true;
    }
  }
}
