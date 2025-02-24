# O2 catalytic converter simulator Arduino/Teensy/ESP32 PIO

## Description
Deleted your catalytic converter for "off road use"? Have a p0420/p0430 check engine code? This project simulates O2 sensor readings using a Teensy or esp32 (or arduino) and resolved that issue by simulating a proper o2 signal. Works for single or multiple sensors(connect all sensors to sim ouput).

**Note:** Must use a PWM pin on controller for output. (default is pin 14)

**Note:** For your application, you may need to adjust the rise and fall times as well as min/max off/on times.[^1]

![O2 simulator](/o2_output.jpg)

[^1]: Voltage and time durration adjustment (Adjustable in **[config.h](./src/config.h)** file📄)

```cpp
// Configurable parameters for the signal simulation
const float maxVoltage = 0.8;  // Target voltage (0.8V)
const float minVoltage = 0.0;  // Off voltage (0V)
const float riseTime = 0.7;    // Rise time in seconds
const float fallTime = 1.1;    // Fall time in seconds

// Configurable range for hold and off times
const float minOnTime = 1.25;  // Minimum hold time in seconds
const float maxOnTime = 10;  // Maximum hold time in seconds
const float minOffTime = 1.25;   // Minimum off time in seconds
const float maxOffTime = 5;   // Maximum off time in seconds
