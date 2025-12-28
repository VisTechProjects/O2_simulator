# O2 catalytic converter simulator Arduino/Teensy/ESP32 PIO

## Description
Deleted your catalytic converter for "off road use"? Have a p0420/p0430 check engine code? This project simulates O2 sensor readings using a Teensy or esp32 (or arduino) and resolved that issue by simulating a proper o2 signal. Works for single or multiple sensors(connect all sensors to sim ouput).

**Note:** Must use a DAC pin on Teensy (default is A14) or PWM+RC filter on ESP32.

**Note:** For your application, you may need to adjust the rise and fall times as well as min/max off/on times.[^1]

## Compatibility
Tested and working on 2007 Lexus IS250. Should work on most vehicles with simple P0420/P0430 detection.

If you get codes on other vehicles, try:
- Longer hold times (more stable signal)
- Narrower voltage range (0.3-0.6V instead of 0-0.8V)
- Slower rise/fall times (1.5-2.0s)

Some newer cars (2015+), German cars (BMW/VW/Audi), and some Hondas use more sophisticated cat monitoring and may not work with a simple simulator.

![O2 simulator](/o2_output.jpg)

[^1]: Voltage and time durration adjustment (Adjustable in **[config.h](./src/config.h)** file📄)

```cpp
// Configurable parameters for the signal simulation
const float maxVoltage = 0.8;  // Rich signal (typical: 0.7-0.9V)
const float minVoltage = 0.0;  // Lean signal (typical: 0.1-0.2V, 0.0 also works)
const float riseTime = 0.7;    // Rise time in seconds (realistic: 1.0-2.0s)
const float fallTime = 1.1;    // Fall time in seconds (realistic: 1.0-2.0s)

// Hold time ranges: How long voltage stays high/low before switching
const float minHighTime = 1.25;  // Minimum time at high voltage
const float maxHighTime = 10;    // Maximum time at high voltage
const float minLowTime = 1.25;   // Minimum time at low voltage
const float maxLowTime = 5;      // Maximum time at low voltage
