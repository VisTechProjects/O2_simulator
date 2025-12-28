#include <Arduino.h>
#include <math.h>  // For sin() and M_PI
#include <config.h>  // Configuration file

//#define debug true

unsigned long previousMillis = 0;
float outputVoltage = 0.0;
int state = 0;  // 0: Off, 1: Rising, 2: Hold, 3: Falling

float highTime = 2.0;  // Randomized high voltage duration
float lowTime = 2.0;   // Randomized low voltage duration

// Function to scale random values with nonlinear distribution
float scaledRandom(float minVal, float maxVal) {
  float r = random(1000) / 1000.0;  // Random value between 0 and 1
  r = r * r;                        // Nonlinear transformation (square)
  return minVal + (maxVal - minVal) * r;
}

void setup() {
  analogWriteResolution(12);  // Set DAC resolution to 12 bits
  analogWrite(outputPin, 0);  // Force 0V output immediately at boot
  pinMode(LED_BUILTIN,OUTPUT);

  #ifdef debug
    Serial.begin(9600);
    Serial.println("Signal Simulation Started");
  #endif

  // Initialize random seed (sample multiple reads for better entropy)
  long seed = 0;
  for (int i = 0; i < 8; i++) {
    seed ^= analogRead(A0) << (i * 2);
    delayMicroseconds(50);
  }
  randomSeed(seed ^ micros());
}

void loop() {
  unsigned long currentMillis = millis();
  float progress;

  switch (state) {
    case 0:  // Off state
      outputVoltage = minVoltage;
      digitalWrite(LED_BUILTIN, LOW);

      // Normal transition OR safety timeout if lowTime is corrupted
      if (currentMillis - previousMillis >= lowTime * 1000 ||
          currentMillis - previousMillis >= (maxLowTime + safetyMargin) * 1000) {
        previousMillis = currentMillis;

        // Randomize the high voltage duration
        highTime = scaledRandom(minHighTime, maxHighTime);
        state = 1;  // Transition to Rising state
        #ifdef debug
          Serial.print("Randomized High Time: ");
          Serial.print(highTime, 2);
          Serial.println(" seconds");
        #endif
      }
      break;

    case 1:  // Rising state
      progress = (currentMillis - previousMillis) / (riseTime * 1000.0);  // 0 to 1
      if (progress >= 1.0) {
        progress = 1.0;
        previousMillis = currentMillis;
        state = 2;  // Transition to Hold state
      }
      outputVoltage = minVoltage + (maxVoltage - minVoltage) * sin(progress * (M_PI / 2));
      break;

    case 2:  // Hold state
      outputVoltage = maxVoltage;
      digitalWrite(LED_BUILTIN, HIGH);

      // Normal transition OR safety timeout if highTime is corrupted
      if (currentMillis - previousMillis >= highTime * 1000 ||
          currentMillis - previousMillis >= (maxHighTime + safetyMargin) * 1000) {
        previousMillis = currentMillis;

        // Randomize the low voltage duration
        lowTime = scaledRandom(minLowTime, maxLowTime);
        state = 3;  // Transition to Falling state

        #ifdef debug
          Serial.print("Randomized Low Time: ");
          Serial.print(lowTime, 2);
          Serial.println(" seconds");
        #endif
      }
      break;

    case 3:  // Falling state
      progress = (currentMillis - previousMillis) / (fallTime * 1000.0);  // 0 to 1
      if (progress >= 1.0) {
        progress = 1.0;
        previousMillis = currentMillis;
        state = 0;  // Transition to Off state
      }
      outputVoltage = maxVoltage - (maxVoltage - minVoltage) * (1 - cos(progress * (M_PI / 2)));
      break;

    default:  // Invalid state - reset to safe state
      state = 0;
      outputVoltage = minVoltage;
      previousMillis = currentMillis;
      break;
  }

  // Scale the output voltage to the DAC value range
  int dacValue = (outputVoltage / dacMaxVoltage) * dacResolution;

  // Ensure the DAC value stays within range
  dacValue = constrain(dacValue, 0, dacResolution);

  // Output the voltage to the DAC pin
  analogWrite(outputPin, dacValue);

  #ifdef debug // Print the current output voltage for debugging
    Serial.print("Output: ");
    Serial.print(outputVoltage, 3);
    Serial.print(" V (DAC: ");
    Serial.print(dacValue);
    Serial.println(")");
  #endif

  delay(10);  // Small delay for stability
}
