/* LEDC Fade Arduino Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// use 12 bit precission for LEDC timer
#define LEDC_TIMER_12_BIT  12

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000

// fade LED PIN (replace with LED_BUILTIN constant for built-in LED)
#define LED_PIN            4

// define starting duty, target duty and maximum fade time
#define LEDC_START_DUTY   (0)
#define LEDC_TARGET_DUTY  (4095)
#define LEDC_FADE_TIME    (3000)

bool fade_ended = false;      // status of LED fade
bool fade_on = true;

void ARDUINO_ISR_ATTR LED_FADE_ISR()
{
  fade_ended = true;
}

void setup() {
  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  while(!Serial) delay(10);

  // Setup timer and attach timer to a led pins
  ledcAttach(LED_PIN, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);

  // Setup and start fade on led (duty from 0 to 4095)
  ledcFade(LED_PIN, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME);
  Serial.println("LED Fade on started.");

  // Wait for fade to end
  delay(LEDC_FADE_TIME);

  // Setup and start fade off led and use ISR (duty from 4095 to 0)
  ledcFadeWithInterrupt(LED_PIN, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR); 
  Serial.println("LED Fade off started.");
}

void loop() {
  // Check if fade_ended flag was set to true in ISR
  if(fade_ended){
    Serial.println("LED fade ended");
    fade_ended = false;

    // Check if last fade was fade on
    if(fade_on){
      ledcFadeWithInterrupt(LED_PIN, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME, LED_FADE_ISR); 
      Serial.println("LED Fade off started.");
      fade_on = false;
    }
    else {
      ledcFadeWithInterrupt(LED_PIN, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR); 
      Serial.println("LED Fade on started.");
      fade_on = true;
    }
  }
}