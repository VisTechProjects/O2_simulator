#include <Arduino.h>
#include <math.h>  // For sin() and M_PI
#include <config.h>  // Configuration file

//#define debug true

unsigned long previousMillis = 0;
float outputVoltage = 0.0;
int state = 0;  // 0: Off, 1: Rising, 2: Hold, 3: Falling

float onTime = 2.0;  // Randomized on time place holder
float offTime = 2.0;   // Randomized off time place holder

// Function to scale random values with nonlinear distribution
float scaledRandom(float minVal, float maxVal) {
  float r = random(1000) / 1000.0;  // Random value between 0 and 1
  r = r * r;                        // Nonlinear transformation (square)
  return minVal + (maxVal - minVal) * r;
}

void setup() {
  analogWriteResolution(12);  // Set DAC resolution to 12 bits
  pinMode(LED_BUILTIN,OUTPUT);

  Serial.begin(9600);
  Serial.println("Signal Simulation Started");

  // Initialize random seed (use analog noise on an unused pin)
  randomSeed(analogRead(A0) + micros());
}

void loop() {
  unsigned long currentMillis = millis();
  float progress;

  switch (state) {
    case 0:  // Off state
      outputVoltage = minVoltage;
      digitalWrite(LED_BUILTIN, LOW);

      if (currentMillis - previousMillis >= offTime * 1000) {
        previousMillis = currentMillis;

        // Randomize the hold time between minOnTime and maxOnTime
        onTime = scaledRandom(minOnTime, maxOnTime);
        state = 1;  // Transition to Rising state
        #ifdef debug
          Serial.print("Randomized Hold On Time: ");
          Serial.print(onTime, 2);
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

      if (currentMillis - previousMillis >= onTime * 1000) {
        previousMillis = currentMillis;

        // Randomize the off time between minOffTime and maxOffTime
        offTime = scaledRandom(minOffTime, maxOffTime);
        state = 3;  // Transition to Falling state

        #ifdef debug
          Serial.print("Randomized Hold Off Time: ");
          Serial.print(offTime, 2);
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
  }

  // Scale the output voltage to the DAC value range
  int dacValue = (outputVoltage / dacMaxVoltage) * dacResolution;

  // Ensure the DAC value stays within range
  dacValue = constrain(dacValue, 0, dacResolution);

  // Output the voltage to the DAC pin
  analogWrite(A14, dacValue);

  #ifdef debug // Print the current output voltage for debugging
    Serial.print("Output Voltage: ");
    Serial.println(dacValue, 2); 
    Serial.println(" V");
  #endif

  delay(10);  // Small delay for stability
}
