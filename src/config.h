// Configurable parameters for the signal simulation
const float maxVoltage = 0.8;  // Target voltage (0.8V)
const float minVoltage = 0.0;  // Off voltage (0V)
const float riseTime = 0.7;    // Rise time in seconds
const float fallTime = 1.1;    // Fall time in seconds

// Configurable range for high and low voltage durations
const float minHighTime = 1.25;  // Minimum time at high voltage (0.8V)
const float maxHighTime = 10;    // Maximum time at high voltage (0.8V)
const float minLowTime = 1.25;   // Minimum time at low voltage (0V)
const float maxLowTime = 5;      // Maximum time at low voltage (0V)

const float safetyMargin = 5;    // Extra seconds before forcing state transition

const float dacMaxVoltage = 3.3;  // Maximum DAC output voltage (3.3V)
const int dacResolution = 4095;  // 12-bit DAC resolution
const int outputPin = A14;       // DAC output pin