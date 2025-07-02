/*
    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
        This will implement the onboard WS2812b LED as a LED indicator
        It can be used to indicate some state or status of the device
        The LED can be controlled using RGB, HSV or color temperature, brightness

        In this example, the LED Indicator class is used as the Matter light accessory
*/

#include "builtinLED.h"

typedef struct {
  uint16_t hue;
  uint8_t saturation;
} HS_color_t;

static const HS_color_t temperatureTable[] = {
  {4, 100},  {8, 100},  {11, 100}, {14, 100}, {16, 100}, {18, 100}, {20, 100}, {22, 100}, {24, 100}, {25, 100}, {27, 100}, {28, 100}, {30, 100}, {31, 100},
  {31, 95},  {30, 89},  {30, 85},  {29, 80},  {29, 76},  {29, 73},  {29, 69},  {28, 66},  {28, 63},  {28, 60},  {28, 57},  {28, 54},  {28, 52},  {27, 49},
  {27, 47},  {27, 45},  {27, 43},  {27, 41},  {27, 39},  {27, 37},  {27, 35},  {27, 33},  {27, 31},  {27, 30},  {27, 28},  {27, 26},  {27, 25},  {27, 23},
  {27, 22},  {27, 21},  {27, 19},  {27, 18},  {27, 17},  {27, 15},  {28, 14},  {28, 13},  {28, 12},  {29, 10},  {29, 9},   {30, 8},   {31, 7},   {32, 6},
  {34, 5},   {36, 4},   {41, 3},   {49, 2},   {0, 0},    {294, 2},  {265, 3},  {251, 4},  {242, 5},  {237, 6},  {233, 7},  {231, 8},  {229, 9},  {228, 10},
  {227, 11}, {226, 11}, {226, 12}, {225, 13}, {225, 13}, {224, 14}, {224, 14}, {224, 15}, {224, 15}, {223, 16}, {223, 16}, {223, 17}, {223, 17}, {223, 17},
  {222, 18}, {222, 18}, {222, 19}, {222, 19}, {222, 19}, {222, 19}, {222, 20}, {222, 20}, {222, 20}, {222, 21}, {222, 21}
};

/* step brightness table: gamma = 2.3 */
static const uint8_t gamma_table[MAX_PROGRESS] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,
  1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   8,
  8,   8,   9,   9,   9,   10,  10,  10,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,
  21,  22,  22,  23,  23,  24,  25,  25,  26,  26,  27,  28,  28,  29,  30,  30,  31,  32,  33,  33,  34,  35,  36,  36,  37,  38,  39,  40,  40,
  41,  42,  43,  44,  45,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,
  69,  70,  71,  72,  74,  75,  76,  77,  78,  79,  81,  82,  83,  84,  86,  87,  88,  89,  91,  92,  93,  95,  96,  97,  99,  100, 101, 103, 104,
  105, 107, 108, 110, 111, 112, 114, 115, 117, 118, 120, 121, 123, 124, 126, 128, 129, 131, 132, 134, 135, 137, 139, 140, 142, 144, 145, 147, 149,
  150, 152, 154, 156, 157, 159, 161, 163, 164, 166, 168, 170, 172, 174, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203,
  205, 207, 209, 211, 213, 215, 217, 219, 221, 223, 226, 228, 230, 232, 234, 236, 239, 241, 243, 245, 248, 250, 252, 255,
};

BuiltInLED::BuiltInLED() {
  pin_number = (uint8_t)-1;  // no pin number
  state = false;             // LED is off
  hsv_color.value = 0;       // black color
}

BuiltInLED::~BuiltInLED() {
  end();
}

led_indicator_color_hsv_t BuiltInLED::rgb2hsv(led_indicator_color_rgb_t rgb) {
  led_indicator_color_hsv_t hsv;
  uint8_t minRGB, maxRGB;
  uint8_t delta;

  minRGB = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
  maxRGB = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
  hsv.value = 0;
  hsv.v = maxRGB;
  delta = maxRGB - minRGB;

  if (delta == 0) {
    hsv.h = 0;
    hsv.s = 0;
  } else {
    hsv.s = delta * 255 / maxRGB;

    if (rgb.r == maxRGB) {
      hsv.h = (60 * (rgb.g - rgb.b) / delta + 360) % 360;
    } else if (rgb.g == maxRGB) {
      hsv.h = (60 * (rgb.b - rgb.r) / delta + 120);
    } else {
      hsv.h = (60 * (rgb.r - rgb.g) / delta + 240);
    }
  }
  return hsv;
}

led_indicator_color_rgb_t BuiltInLED::hsv2rgb(led_indicator_color_hsv_t hsv) {
  led_indicator_color_rgb_t rgb;
  uint8_t rgb_max = hsv.v;
  uint8_t rgb_min = rgb_max * (255 - hsv.s) / 255.0f;

  uint8_t i = hsv.h / 60;
  uint8_t diff = hsv.h % 60;

  // RGB adjustment amount by hue
  uint8_t rgb_adj = (rgb_max - rgb_min) * diff / 60;
  rgb.value = 0;
  switch (i) {
    case 0:
      rgb.r = rgb_max;
      rgb.g = rgb_min + rgb_adj;
      rgb.b = rgb_min;
      break;
    case 1:
      rgb.r = rgb_max - rgb_adj;
      rgb.g = rgb_max;
      rgb.b = rgb_min;
      break;
    case 2:
      rgb.r = rgb_min;
      rgb.g = rgb_max;
      rgb.b = rgb_min + rgb_adj;
      break;
    case 3:
      rgb.r = rgb_min;
      rgb.g = rgb_max - rgb_adj;
      rgb.b = rgb_max;
      break;
    case 4:
      rgb.r = rgb_min + rgb_adj;
      rgb.g = rgb_min;
      rgb.b = rgb_max;
      break;
    default:
      rgb.r = rgb_max;
      rgb.g = rgb_min;
      rgb.b = rgb_max - rgb_adj;
      break;
  }

  // gamma correction
  rgb.r = gamma_table[rgb.r];
  rgb.g = gamma_table[rgb.g];
  rgb.b = gamma_table[rgb.b];
  return rgb;
}

void BuiltInLED::begin(uint8_t pin) {
  if (pin < NUM_DIGITAL_PINS) {
    pin_number = pin;
    log_i("Initializing pin %d", pin);
  } else {
    log_e("Invalid pin (%d) number", pin);
  }
}
void BuiltInLED::end() {
  state = false;
  write();  // turn off the LED
  if (pin_number < NUM_DIGITAL_PINS) {
    if (!rmtDeinit(pin_number)) {
      log_e("Failed to deinitialize RMT");
    }
  }
}

void BuiltInLED::on() {
  state = true;
}

void BuiltInLED::off() {
  state = false;
}

void BuiltInLED::toggle() {
  state = !state;
}

bool BuiltInLED::getState() {
  return state;
}

bool BuiltInLED::write() {
  led_indicator_color_rgb_t rgb_color = getRGB();
  log_d("Writing to pin %d with state = %s", pin_number, state ? "ON" : "OFF");
  log_d("HSV: %d, %d, %d", hsv_color.h, hsv_color.s, hsv_color.v);
  log_d("RGB: %d, %d, %d", rgb_color.r, rgb_color.g, rgb_color.b);
  if (pin_number < NUM_DIGITAL_PINS) {
    if (state) {
      rgbLedWrite(pin_number, rgb_color.r, rgb_color.g, rgb_color.b);
    } else {
      rgbLedWrite(pin_number, 0, 0, 0);
    }
    return true;
  } else {
    log_e("Invalid pin (%d) number", pin_number);
    return false;
  }
}

void BuiltInLED::setBrightness(uint8_t brightness) {
  hsv_color.v = brightness;
}

uint8_t BuiltInLED::getBrightness() {
  return hsv_color.v;
}

void BuiltInLED::setHSV(led_indicator_color_hsv_t hsv) {
  if (hsv.h > MAX_HUE) {
    hsv.h = MAX_HUE;
  }
  hsv_color.value = hsv.value;
}

led_indicator_color_hsv_t BuiltInLED::getHSV() {
  return hsv_color;
}

void BuiltInLED::setRGB(led_indicator_color_rgb_t rgb_color) {
  hsv_color = rgb2hsv(rgb_color);
}

led_indicator_color_rgb_t BuiltInLED::getRGB() {
  return hsv2rgb(hsv_color);
}

void BuiltInLED::setTemperature(uint32_t temperature) {
  uint16_t hue;
  uint8_t saturation;

  log_d("Requested Temperature: %ld", temperature);
  //hsv_color.v = gamma_table[((temperature >> 25) & 0x7F)];
  temperature &= 0xFFFFFF;
  if (temperature < 600) {
    hue = 0;
    saturation = 100;
  } else {
    if (temperature > 10000) {
      hue = 222;
      saturation = 21 + (temperature - 10000) * 41 / 990000;
    } else {
      temperature -= 600;
      temperature /= 100;
      hue = temperatureTable[temperature].hue;
      saturation = temperatureTable[temperature].saturation;
    }
  }
  saturation = (saturation * 255) / 100;
  // brightness is not changed
  hsv_color.h = hue;
  hsv_color.s = saturation;
  log_d("Calculated Temperature: %ld, Hue: %d, Saturation: %d, Brightness: %d", temperature, hue, saturation, hsv_color.v);
}
