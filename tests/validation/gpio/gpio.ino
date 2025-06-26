#include <Arduino.h>
#include <unity.h>


#define BTN 0
#define LED 4

volatile int interruptCounter = 0;
volatile bool interruptFlag = false;
volatile unsigned long lastInterruptTime = 0;

// Variables for interrupt with argument test
volatile int argInterruptCounter = 0;
volatile bool argInterruptFlag = false;
volatile int receivedArg = 0;

void setUp(void) {
  interruptCounter = 0;
  interruptFlag = false;
  lastInterruptTime = 0;
  argInterruptCounter = 0;
  argInterruptFlag = false;
  receivedArg = 0;
}

void tearDown(void) {
  detachInterrupt(digitalPinToInterrupt(BTN));
}

void IRAM_ATTR buttonISR() {
  unsigned long currentTime = millis();
  // Simple debouncing - ignore interrupts within 50ms
  if (currentTime - lastInterruptTime > 50) {
    interruptCounter = interruptCounter + 1;
    interruptFlag = true;
    lastInterruptTime = currentTime;
  }
}

void IRAM_ATTR buttonISRWithArg(void *arg) {
  unsigned long currentTime = millis();
  // Simple debouncing - ignore interrupts within 50ms
  if (currentTime - lastInterruptTime > 50) {
    argInterruptCounter = argInterruptCounter + 1;
    argInterruptFlag = true;
    receivedArg = *(int*)arg;
    lastInterruptTime = currentTime;
  }
}

void test_interrupt_attach_detach(void) {
  Serial.println("GPIO interrupt - attach/detach test START");

  pinMode(BTN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  // Reset counters
  interruptCounter = 0;
  interruptFlag = false;

  // Attach interrupt on falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, FALLING);
  Serial.println("Interrupt attached - FALLING edge");

  // Wait for first button press
  delay(1000);
  Serial.println("Press button to trigger interrupt");

  // Wait for interrupt to be triggered
  unsigned long startTime = millis();
  while (!interruptFlag && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_TRUE(interruptFlag);
  TEST_ASSERT_EQUAL(1, interruptCounter);
  Serial.println("First interrupt triggered successfully");

  // Reset flag for next test
  interruptFlag = false;

  // Wait for second button press
  delay(1000);
  Serial.println("Press button again to trigger second interrupt");

  startTime = millis();
  while (!interruptFlag && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_TRUE(interruptFlag);
  TEST_ASSERT_EQUAL(2, interruptCounter);
  Serial.println("Second interrupt triggered successfully");

  // Now detach interrupt
  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Interrupt detached");

  // Reset counters and test that interrupt no longer works
  interruptCounter = 0;
  interruptFlag = false;

  delay(1000);
  Serial.println("Press button - should NOT trigger interrupt");

  // Wait and verify no interrupt occurs
  delay(3000);
  TEST_ASSERT_FALSE(interruptFlag);
  TEST_ASSERT_EQUAL(0, interruptCounter);
  Serial.println("No interrupt triggered after detach - SUCCESS");
}

void test_interrupt_rising_falling(void) {
  Serial.println("GPIO interrupt - rising/falling edge test START");

  pinMode(BTN, INPUT_PULLUP);

  // Test FALLING edge
  interruptCounter = 0;
  interruptFlag = false;

  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, FALLING);
  Serial.println("Testing FALLING edge interrupt");

  delay(1000);
  Serial.println("Press button for FALLING edge test");

  unsigned long startTime = millis();
  while (!interruptFlag && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_TRUE(interruptFlag);
  Serial.println("FALLING edge interrupt worked");

  detachInterrupt(digitalPinToInterrupt(BTN));

  // Test RISING edge
  interruptCounter = 0;
  interruptFlag = false;

  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, RISING);
  Serial.println("Testing RISING edge interrupt");

  delay(1000);
  Serial.println("Release button for RISING edge test");

  startTime = millis();
  while (!interruptFlag && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_TRUE(interruptFlag);
  Serial.println("RISING edge interrupt worked");

  detachInterrupt(digitalPinToInterrupt(BTN));
}

void test_interrupt_change(void) {
  Serial.println("GPIO interrupt - CHANGE edge test START");

  pinMode(BTN, INPUT_PULLUP);

  interruptCounter = 0;
  interruptFlag = false;

  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, CHANGE);
  Serial.println("Testing CHANGE edge interrupt");

  delay(1000);
  Serial.println("Press and release button for CHANGE test");

  // Wait for button press (falling edge)
  unsigned long startTime = millis();
  while (interruptCounter < 1 && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_GREATER_OR_EQUAL(1, interruptCounter);
  Serial.println("Button press detected");

  // Wait for button release (rising edge)
  startTime = millis();
  while (interruptCounter < 2 && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_GREATER_OR_EQUAL(2, interruptCounter);
  Serial.println("Button release detected - CHANGE interrupt worked");

  detachInterrupt(digitalPinToInterrupt(BTN));
}

void test_interrupt_with_arg(void) {
  Serial.println("GPIO interrupt - attachInterruptArg test START");

  pinMode(BTN, INPUT_PULLUP);

  // Test data to pass to interrupt
  int testArg = 42;

  // Reset counters
  argInterruptCounter = 0;
  argInterruptFlag = false;
  receivedArg = 0;

  // Attach interrupt with argument on falling edge (button press)
  attachInterruptArg(digitalPinToInterrupt(BTN), buttonISRWithArg, &testArg, FALLING);
  Serial.println("Interrupt with argument attached - FALLING edge");

  delay(1000);
  Serial.println("Press button to trigger interrupt with argument");

  // Wait for interrupt to be triggered
  unsigned long startTime = millis();
  while (!argInterruptFlag && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_TRUE(argInterruptFlag);
  TEST_ASSERT_EQUAL(1, argInterruptCounter);
  TEST_ASSERT_EQUAL(42, receivedArg);
  Serial.println("Interrupt with argument triggered successfully");
  Serial.print("Received argument value: ");
  Serial.println(receivedArg);

  // Test with different argument value
  argInterruptFlag = false;
  int newTestArg = 123;

  // Detach and reattach with new argument
  detachInterrupt(digitalPinToInterrupt(BTN));
  attachInterruptArg(digitalPinToInterrupt(BTN), buttonISRWithArg, &newTestArg, FALLING);
  Serial.println("Interrupt reattached with new argument value");

  delay(1000);
  Serial.println("Press button again to test new argument");

  startTime = millis();
  while (!argInterruptFlag && (millis() - startTime < 5000)) {
    delay(10);
  }

  TEST_ASSERT_TRUE(argInterruptFlag);
  TEST_ASSERT_EQUAL(2, argInterruptCounter);
  TEST_ASSERT_EQUAL(123, receivedArg);
  Serial.println("Second interrupt with new argument triggered successfully");
  Serial.print("New received argument value: ");
  Serial.println(receivedArg);

  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Interrupt with argument test completed");
}


void test_read_basic(void) {
  pinMode(BTN, INPUT_PULLUP);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(BTN));
  Serial.println("BTN read as HIGH after pinMode INPUT_PULLUP");

  delay(1000);
  TEST_ASSERT_EQUAL(LOW, digitalRead(BTN));
  Serial.println("BTN read as LOW");

  delay(1000);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(BTN));
  Serial.println("BTN read as HIGH");
}

void test_write_basic(void) {
  Serial.println("GPIO write - basic START");
  pinMode(LED, OUTPUT);
  delay(1000);
  Serial.println("GPIO LED set to OUTPUT");
  delay(2000);

  digitalWrite(LED, HIGH);
  delay(1000);
  Serial.println("LED set to HIGH");

  delay(3000);
  digitalWrite(LED, LOW);
  Serial.println("LED set to LOW");
}


void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  UNITY_BEGIN();

  Serial.println("GPIO interrupt test START");
  RUN_TEST(test_interrupt_attach_detach);
  RUN_TEST(test_interrupt_rising_falling);
  RUN_TEST(test_interrupt_change);
  RUN_TEST(test_interrupt_with_arg);
  Serial.println("GPIO interrupt test END");

  Serial.println("GPIO basic test START");
  RUN_TEST(test_read_basic);
  RUN_TEST(test_write_basic);
  Serial.println("GPIO basic test END");

  UNITY_END();
  Serial.println("GPIO test END");
}

void loop() {}
