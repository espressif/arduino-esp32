# ESP32 Lambda FunctionalInterrupt Example

This example demonstrates how to use lambda functions with FunctionalInterrupt for GPIO pin interrupt callbacks on ESP32. It showcases different lambda patterns and capture techniques for interrupt handling with advanced debouncing through two comprehensive examples:

**Example 1**: CHANGE mode lambda with object method integration, state-based debouncing, and dual-edge detection  
**Example 2**: FALLING mode lambda with pointer captures, LED control, and comprehensive variable capture patterns

## Features Demonstrated

1. **CHANGE mode lambda** to handle both RISING and FALLING edges on same pin
2. **Lambda function with captured variables** (pointers)
3. **Object method calls integrated within lambda functions**
4. **Edge type detection** using digitalRead() within ISR
5. **Hardware debouncing** with configurable timeout and anti-hysteresis
6. **Best practices** for interrupt-safe lambda functions
7. **Cross-platform terminal compatibility** (Windows, Linux, macOS)

## Hardware Setup

- Connect a button between GPIO 4 and GND (with internal pullup)
- Connect an LED with resistor to GPIO 2 (built-in LED on most ESP32 boards)
- Optionally connect a second button to BOOT pin for advanced examples

```
ESP32 Board          Button 1        Button 2 (Optional)    LED
───────────          ────────        ──────────────────     ───
GPIO 4 ──────────── [BUTTON] ──── GND
GPIO 0 (BOOT) ────── [BUTTON] ──── GND
GPIO 2 ──────────── [LED] ──────── GND
                       │
                     [330Ω]
                       │
                      3V3
```

## Important ESP32 Interrupt Behavior

**CRITICAL:** Only ONE interrupt handler can be attached per GPIO pin at a time on ESP32.

- Calling `attachInterrupt()` on a pin that already has an interrupt will **override** the previous one
- This applies regardless of edge type (RISING, FALLING, CHANGE)
- If you need both RISING and FALLING detection on the same pin, use **CHANGE mode** and determine the edge type within your handler by reading the pin state

## Code Examples Overview

This example contains **two main implementations** that demonstrate different lambda interrupt patterns:

### Example 1: CHANGE Mode Lambda (Button 1 - GPIO 4)
- **Pattern**: Comprehensive CHANGE mode with edge detection
- **Features**: Dual-edge detection (RISING/FALLING), object method calls, state-based debouncing
- **Use Case**: Single pin handling both button press and release events
- **Lambda Type**: Complex lambda with multiple pointer captures and object interaction

### Example 2: FALLING Mode Lambda (Button 2 - BOOT Pin)  
- **Pattern**: Pointer capture lambda with LED control
- **Features**: Single-edge detection, extensive variable capture, hardware control
- **Use Case**: Traditional button press with peripheral control (LED toggle)
- **Lambda Type**: Capture-heavy lambda demonstrating safe variable access patterns

## Lambda Function Patterns Explained

### 1. CHANGE Mode Lambda (Recommended for Multiple Edges)
```cpp
attachInterrupt(pin, lambda, CHANGE);
```
- Detects both RISING and FALLING edges on the same pin
- Use `digitalRead()` inside the lambda to determine edge type
- Most efficient way to handle both press and release events
- Avoids the "handler override" issue entirely

### 2. Pointer Capture Lambda
```cpp
[ptr1, ptr2]() { *ptr1 = value; }
```
- Captures pointers to global/static variables
- Avoids compiler warnings about non-automatic storage duration
- Allows safe modification of external variables in ISR

### 3. Object Method Lambda
```cpp
[objectPtr]() { objectPtr->method(); }
```
- Captures pointer to object instance
- Enables calling object methods from interrupt
- Useful for object-oriented interrupt handling

### 4. State-Based Debouncing Pattern (Anti-Hysteresis)
```cpp
bool currentState = digitalRead(pin);
if (currentState == lastKnownState) return;  // No real change
lastKnownState = currentState;
```
- Prevents counting multiple edges from electrical noise/hysteresis
- Ensures press/release counts remain synchronized
- Tracks actual state changes rather than just edge triggers
- Critical for reliable button input with mechanical switches

### 5. Time-Based Debouncing Pattern
```cpp
unsigned long currentTime = millis();
if (currentTime - lastInterruptTime < DEBOUNCE_DELAY) return;
lastInterruptTime = currentTime;
```
- Prevents multiple triggers from mechanical switch bounce
- Uses `millis()` for timing (fast and ISR-safe)
- Typical debounce delay: 20-100ms depending on switch quality

## Edge Detection Pattern in CHANGE Mode

```cpp
if (digitalRead(pin) == LOW) {
  // FALLING edge detected (button pressed)
} else {
  // RISING edge detected (button released)
}
```

## Best Practices for Lambda Interrupts

✅ **Keep lambda functions short and simple**  
✅ **Use volatile for variables modified in ISR**  
✅ **Avoid complex operations** (delays, Serial.print, etc.)  
✅ **NEVER call digitalWrite() or other GPIO functions directly in ISR**  
✅ **Use flags and handle GPIO operations in main loop**  
✅ **Use pointer captures for static/global variables**  
✅ **Use std::function<void()> for type clarity**  
✅ **Implement state-based debouncing to prevent hysteresis issues**  
✅ **Track last known pin state to avoid counting noise as real edges**  
✅ **Test thoroughly with real hardware**  
✅ **Consider using flags and processing in loop()**  

## Lambda Capture Syntax Guide

| Syntax          | Description                          |
|-----------------|--------------------------------------|
| `[]`            | Capture nothing                      |
| `[var]`         | Capture var by value (copy)         |
| `[&var]`        | Capture var by reference (avoid!)   |
| `[ptr]`         | Capture pointer (recommended)       |
| `[=]`           | Capture all by value                 |
| `[&]`           | Capture all by reference (avoid!)   |
| `[var1, ptr2]`  | Mixed captures                       |

## Why Use Pointer Captures?

• **Avoids "non-automatic storage duration" compiler warnings**  
• **More explicit about what's being accessed**  
• **Better performance than reference captures**  
• **Safer for static and global variables**  
• **Compatible with all ESP32 compiler versions**  

## Debouncing Strategy

This example implements a **dual-layer debouncing approach**:

1. **Time-based debouncing**: Ignores interrupts within 50ms of the previous one
2. **State-based debouncing**: Only processes interrupts that represent actual state changes

This combination effectively eliminates:
- Mechanical switch bounce
- Electrical noise and hysteresis
- Multiple false triggers
- Synchronized press/release count mismatches

## Expected Output

```
ESP32 Lambda FunctionalInterrupt Example
========================================
Setting up Example 1: CHANGE mode lambda for both edges
Setting up Example 2: Lambda with pointer captures

Lambda interrupts configured:
- Button 1 (Pin 4): CHANGE mode lambda (handles both press and release)
- Button 2 (Pin 0): FALLING mode lambda with LED control
- Debounce delay: 50 ms for both buttons

Press and release the buttons to see lambda interrupts in action!
Button 1 will detect both press (FALLING) and release (RISING) events.
Button 2 (FALLING only) will toggle the LED.
Both buttons include debouncing to prevent mechanical bounce issues.

==> Button 1 PRESSED! Count: 1 (FALLING edge detected)
Handler 'ButtonHandler': Press count = 1
==> Button 1 RELEASED! Count: 1 (RISING edge detected)
Handler 'ButtonHandler': Press count = 2
==> Button 2 pressed! Count: 1, LED: ON (Capture lambda)

============================
Lambda Interrupt Statistics:
============================
Button 1 presses:            2
Button 1 releases:           2
Button 2 presses:            1
Object handler calls:        2
Total interrupts:            1
LED state:                  ON
Last interrupt:           1234 ms ago
```

## Pin Configuration

The example uses these default pins (configurable in the source):

- `BUTTON_PIN`: GPIO 4 (change as needed)
- `BUTTON2_PIN`: BOOT_PIN (GPIO 0 on most ESP32 boards)
- `LED_PIN`: LED_BUILTIN (GPIO 2 on most ESP32 boards)

## Compilation Notes

This example uses proper pointer captures for static/global variables to avoid compiler warnings about non-automatic storage duration.