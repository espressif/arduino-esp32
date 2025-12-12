# ESP32 Lambda FunctionalInterrupt Example

This example demonstrates how to use lambda functions with FunctionalInterrupt for GPIO pin interrupt callbacks on ESP32. It shows CHANGE mode detection with LED toggle functionality and proper debouncing.

## Features Demonstrated

1. **CHANGE mode lambda** to detect both RISING and FALLING edges
2. **LED toggle on button press** (FALLING edge)
3. **Edge type detection** using digitalRead() within ISR
4. **Hardware debouncing** with configurable timeout
5. **IRAM_ATTR lambda declaration** for optimal ISR performance in RAM

## Hardware Setup

- Use BOOT Button or connect a button between BUTTON_PIN and GND (with internal pullup)
- Use Builtin Board LED (no special hardware setup) or connect an LED with resistor to GPIO assigned as LED_PIN.\
  Some boards have an RGB LED that needs no special hardware setup to work as a simple white on/off LED.

```
ESP32 Board          Button/LED
-----------          ---------
BOOT_PIN ------------ [BUTTON] ---- GND
LED_PIN --------------- [LED] ----- GND
                          ¦
                        [330O] (*) Only needed when using an external LED attached to the GPIO.
                          ¦
                         3V3
```

## Important ESP32 Interrupt Behavior

**CRITICAL:** Only ONE interrupt handler can be attached per GPIO pin at a time on ESP32.

- Calling `attachInterrupt()` on a pin that already has an interrupt will **override** the previous one
- This applies regardless of edge type (RISING, FALLING, CHANGE)
- If you need both RISING and FALLING detection on the same pin, use **CHANGE mode** and determine the edge type within your handler by reading the pin state

## Code Overview

This example demonstrates a simple CHANGE mode lambda interrupt that:

- **Detects both button press and release** using a single interrupt handler
- **Toggles LED only on button press** (FALLING edge)
- **Reports both press and release events** to Serial output
- **Uses proper debouncing** to prevent switch bounce issues
- **Implements minimal lambda captures** for simplicity

## Lambda Function Pattern

### CHANGE Mode Lambda with IRAM Declaration
```cpp
// Global lambda declared with IRAM_ATTR for optimal ISR performance
IRAM_ATTR std::function<void()> changeModeLambda = []() {
    // Debouncing check
    unsigned long currentTime = millis();
    if (currentTime - lastButtonInterruptTime < DEBOUNCE_DELAY_MS) {
        return; // Ignore bouncing
    }

    // Determine edge type
    bool currentState = digitalRead(BUTTON_PIN);
    if (currentState == lastButtonState) {
        return; // No real state change
    }

    // Update state and handle edges
    lastButtonInterruptTime = currentTime;
    lastButtonState = currentState;

    if (currentState == LOW) {
        // Button pressed (FALLING edge)
        buttonPressCount++;
        buttonPressed = true;
        ledStateChanged = true;  // Signal LED toggle
    } else {
        // Button released (RISING edge)  
        buttonReleaseCount++;
        buttonReleased = true;
    }
};

attachInterrupt(BUTTON_PIN, changeModeLambda, CHANGE);
```

## Key Concepts

### Edge Detection in CHANGE Mode
```cpp
if (digitalRead(pin) == LOW) {
  // FALLING edge detected (button pressed)
} else {
  // RISING edge detected (button released)
}
```

### Debouncing Strategy
This example implements dual-layer debouncing:
1. **Time-based**: Ignores interrupts within 50 ms of previous one
2. **State-based**: Only processes actual state changes

### Main Loop Processing
```cpp
void loop() {
  // Handle LED changes safely outside ISR
  if (ledStateChanged) {
    ledStateChanged = false;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  // Report button events
  if (buttonPressed) {
    // Handle press event
  }
  if (buttonReleased) {
    // Handle release event  
  }
}
```

## Expected Output

```
ESP32 Lambda FunctionalInterrupt Example
========================================
Setting up CHANGE mode lambda for LED toggle

Lambda interrupt configured on Pin 0 (CHANGE mode)
Debounce delay: 50 ms

Press the button to toggle the LED!
Button press (FALLING edge) will toggle the LED.
Button release (RISING edge) will be detected and reported.
Button includes debouncing to prevent mechanical bounce issues.

==> Button PRESSED! Count: 1, LED: ON (FALLING edge)
==> Button RELEASED! Count: 1 (RISING edge)
==> Button PRESSED! Count: 2, LED: OFF (FALLING edge)
==> Button RELEASED! Count: 2 (RISING edge)
```

## Pin Configuration

The example uses these default pins:

- `BUTTON_PIN`: BOOT_PIN (automatically assigned by the Arduino Core)
- `LED_PIN`: LED_BUILTIN (may not be available for your board - please verify it)
