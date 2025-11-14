/**
 * GPIO Validation Test
 * There are multiple synchronization points in this test.
 * They are required for proper timing between the running code and wokwi-pytest.
 * Without them, the test sometimes fails due to timing issues.
 */

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

void waitForSyncAck(const String &token = "OK") {
  while (true) {
    String response = Serial.readStringUntil('\n');
    response.trim();
    if (response.equalsIgnoreCase(token)) {
      break;
    }
    delay(10);
  }
}

void setUp(void) {
  interruptCounter = 0;
  interruptFlag = false;
  lastInterruptTime = 0;
  argInterruptCounter = 0;
  argInterruptFlag = false;
  receivedArg = 0;
}

void tearDown(void) {}

void IRAM_ATTR buttonISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > 50) {
    interruptCounter = interruptCounter + 1;
    interruptFlag = true;
    lastInterruptTime = currentTime;
  }
}

void IRAM_ATTR buttonISRWithArg(void *arg) {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > 50) {
    argInterruptCounter = argInterruptCounter + 1;
    argInterruptFlag = true;
    receivedArg = *(int *)arg;
    lastInterruptTime = currentTime;
  }
}

void test_read_basic(void) {
  pinMode(BTN, INPUT_PULLUP);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(BTN));
  Serial.println("BTN read as HIGH after pinMode INPUT_PULLUP");

  waitForSyncAck();  // sync ack R1
  TEST_ASSERT_EQUAL(LOW, digitalRead(BTN));
  Serial.println("BTN read as LOW");

  waitForSyncAck();  // sync ack R2
  TEST_ASSERT_EQUAL(HIGH, digitalRead(BTN));
  Serial.println("BTN read as HIGH");
}

void test_write_basic(void) {
  pinMode(LED, OUTPUT);
  Serial.println("GPIO LED set to OUTPUT");
  waitForSyncAck();  // sync ack W1

  digitalWrite(LED, HIGH);
  Serial.println("LED set to HIGH");
  waitForSyncAck();  // sync ack W2

  digitalWrite(LED, LOW);
  Serial.println("LED set to LOW");
}

void test_interrupt_attach_detach(void) {
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  interruptCounter = 0;
  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, FALLING);
  Serial.println("Interrupt attached - FALLING edge");

  for (int i = 1; i <= 3; i++) {
    interruptFlag = false;
    waitForSyncAck("OK:" + String(i));

    TEST_ASSERT_TRUE(interruptFlag);
    TEST_ASSERT_EQUAL(i, interruptCounter);
    Serial.println(String(i) + " interrupt triggered successfully");
  }

  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Interrupt detached");

  interruptCounter = 0;
  interruptFlag = false;

  TEST_ASSERT_FALSE(interruptFlag);
  TEST_ASSERT_EQUAL(0, interruptCounter);
  Serial.println("No interrupt triggered after detach");
  waitForSyncAck();
}

void test_interrupt_falling(void) {
  pinMode(BTN, INPUT_PULLUP);

  interruptCounter = 0;
  interruptFlag = false;

  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, FALLING);
  Serial.println("Testing FALLING edge interrupt");

  for (int i = 1; i <= 3; i++) {
    interruptFlag = false;
    waitForSyncAck("OK:" + String(i));

    TEST_ASSERT_TRUE(interruptFlag);
    TEST_ASSERT_EQUAL(i, interruptCounter);
    Serial.println(String(i) + " FALLING edge interrupt worked");
  }

  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Testing FALLING edge END");
  waitForSyncAck();
}

void test_interrupt_rising(void) {
  pinMode(BTN, INPUT_PULLUP);

  interruptCounter = 0;
  interruptFlag = false;

  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, RISING);
  Serial.println("Testing RISING edge interrupt");

  interruptCounter = 0;
  interruptFlag = false;

  for (int i = 1; i <= 3; i++) {
    interruptFlag = false;
    waitForSyncAck("OK:" + String(i));

    TEST_ASSERT_TRUE(interruptFlag);
    TEST_ASSERT_EQUAL(i, interruptCounter);
    Serial.println(String(i) + " RISING edge interrupt worked");
  }

  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Testing RISING edge END");
  waitForSyncAck();
}

void test_interrupt_change(void) {
  pinMode(BTN, INPUT_PULLUP);

  interruptCounter = 0;
  interruptFlag = false;

  attachInterrupt(digitalPinToInterrupt(BTN), buttonISR, CHANGE);
  Serial.println("Testing CHANGE edge interrupt");

  for (int i = 1; i <= 6; i++) {
    interruptFlag = false;
    waitForSyncAck("OK:" + String(i));

    TEST_ASSERT_TRUE(interruptFlag);
    TEST_ASSERT_EQUAL(i, interruptCounter);
    Serial.println(String(i) + " CHANGE edge interrupt worked");
  }

  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Testing CHANGE edge END");
  waitForSyncAck();
}

void test_interrupt_with_arg(void) {
  pinMode(BTN, INPUT_PULLUP);

  int argValue = 42;  // Example argument to pass
  interruptCounter = 0;
  argInterruptFlag = false;

  attachInterruptArg(digitalPinToInterrupt(BTN), buttonISRWithArg, &argValue, FALLING);
  Serial.println("Testing interrupt with argument");

  for (int i = 1; i <= 3; i++) {
    argInterruptFlag = false;
    waitForSyncAck("OK:" + String(i));

    TEST_ASSERT_TRUE(argInterruptFlag);
    TEST_ASSERT_EQUAL(i, argInterruptCounter);
    TEST_ASSERT_EQUAL(argValue, receivedArg);
    Serial.println(String(i) + " interrupt with argument worked, received arg: " + String(receivedArg));
    ++argValue;
  }

  detachInterrupt(digitalPinToInterrupt(BTN));
  Serial.println("Testing interrupt with argument END");
  waitForSyncAck();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  UNITY_BEGIN();

  Serial.println("GPIO test START");
  RUN_TEST(test_read_basic);
  RUN_TEST(test_write_basic);

  Serial.println("GPIO interrupt START");
  RUN_TEST(test_interrupt_attach_detach);
  RUN_TEST(test_interrupt_falling);
  RUN_TEST(test_interrupt_rising);

  RUN_TEST(test_interrupt_change);
  RUN_TEST(test_interrupt_with_arg);

  UNITY_END();
  Serial.println("GPIO test END");
}

void loop() {}
