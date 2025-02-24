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


const float dacMaxVoltage = 3.3;  // Maximum DAC output voltage (3.3V)
const int dacResolution = 4095;  // 12-bit DAC resolution