# O2 Simulator - Future Improvements

## Signal Quality
- Switch from 8-bit DAC (`dacWrite`) to LEDC PWM (`ledcWrite`) for 12-bit resolution
  - Requires RC low-pass filter on output pin (e.g. 1k resistor + 1uF cap)
  - Current 8-bit DAC gives ~62 usable steps for 0-0.8V range (13mV steps)
  - 12-bit would give ~993 steps (0.8mV steps), matching original Teensy quality
  - PWM_FREQ and PWM_RESOLUTION already defined in config.h (5kHz, 12-bit)

## Thread Safety
- Add mutex around `config` struct reads/writes to prevent rare glitches during web config updates
- Add synchronization on voltage history ring buffer for cleaner web graph data

## Power / Hardware
- Test behavior during cold cranking voltage drops (6-9V at battery)
  - ESP32 brownout detector should cleanly reset instead of locking up like Teensy
  - Consider adding bulk capacitor (470uF+) on USB charger input for voltage dip ride-through
  - Consider supercap on 3.3V rail for extra brownout protection

## Minor
- Remove unused `PWM_FREQ` and `PWM_RESOLUTION` from config.h if staying with DAC
- Clean up `volatile` usage - not strictly needed for multi-core (atomics or mutex would be more correct)
