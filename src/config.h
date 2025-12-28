// Configurable parameters for the signal simulation
// Voltage range: Real downstream O2 sensors typically swing 0.1-0.9V
// Current values work well for 2007 Lexus IS250
const float maxVoltage = 0.8;  // Rich signal (typical: 0.7-0.9V)
const float minVoltage = 0.0;  // Lean signal (typical: 0.1-0.2V, 0.0 also works)

// Transition times: Downstream sensors are sluggish due to cat smoothing
const float riseTime = 0.7;    // Rise time in seconds (realistic: 1.0-2.0s)
const float fallTime = 1.1;    // Fall time in seconds (realistic: 1.0-2.0s)

// Hold time ranges: How long voltage stays high/low before switching
// Longer = more stable signal (more like healthy cat)
const float minHighTime = 1.25;  // Minimum time at high voltage
const float maxHighTime = 10;    // Maximum time at high voltage
const float minLowTime = 1.25;   // Minimum time at low voltage
const float maxLowTime = 5;      // Maximum time at low voltage

const float safetyMargin = 5;    // Extra seconds before forcing state transition

const float dacMaxVoltage = 3.3;  // Maximum DAC output voltage (3.3V)
const int dacResolution = 4095;  // 12-bit DAC resolution
const int outputPin = A14;       // DAC output pin