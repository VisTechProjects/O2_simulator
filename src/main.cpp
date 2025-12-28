#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <math.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "ota_updater.h"

// Watchdog timeout (seconds) - reboots if main loop hangs
#define WDT_TIMEOUT 30

// Firmware version
#define FIRMWARE_VERSION "1.0.0"

// NVS storage for config backup (survives SPIFFS updates)
Preferences preferences;

// Output enabled flag (for on/off toggle)
volatile bool outputEnabled = true;

// WiFi reconnect timing
unsigned long lastWifiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000;  // Check every 10 seconds

// mDNS re-announcement timing
unsigned long lastMdnsAnnounce = 0;
const unsigned long MDNS_ANNOUNCE_INTERVAL = 30000;  // Re-announce every 30 seconds

// Global config (modifiable via web)
Config config;

// Signal generation state
volatile unsigned long previousMillis = 0;
volatile float outputVoltage = 0.0;
volatile int state = 0;  // 0: Low, 1: Rising, 2: High, 3: Falling
volatile float highTime = 2.0;
volatile float lowTime = 2.0;

// Voltage history buffer for live trace (100 samples @ 5ms = 500ms of data)
#define HISTORY_SIZE 100
volatile float voltageHistory[HISTORY_SIZE];
volatile uint8_t stateHistory[HISTORY_SIZE];  // State for color-coding
volatile int historyIndex = 0;
volatile unsigned long sampleCounter = 0;  // Global sample counter for sync

// Web server
AsyncWebServer server(80);

// Config file path
const char* CONFIG_FILE = "/config.json";

// Save config to NVS (survives SPIFFS updates)
void saveConfigToNVS() {
  preferences.begin("o2config", false);
  preferences.putFloat("maxVoltage", config.maxVoltage);
  preferences.putFloat("minVoltage", config.minVoltage);
  preferences.putFloat("riseTime", config.riseTime);
  preferences.putFloat("fallTime", config.fallTime);
  preferences.putFloat("minHighTime", config.minHighTime);
  preferences.putFloat("maxHighTime", config.maxHighTime);
  preferences.putFloat("minLowTime", config.minLowTime);
  preferences.putFloat("maxLowTime", config.maxLowTime);
  preferences.end();
  Serial.println("Config saved to NVS");
}

// Load config from NVS
bool loadConfigFromNVS() {
  preferences.begin("o2config", true);
  if (!preferences.isKey("maxVoltage")) {
    preferences.end();
    Serial.println("No NVS config found");
    return false;
  }
  config.maxVoltage = preferences.getFloat("maxVoltage", config.maxVoltage);
  config.minVoltage = preferences.getFloat("minVoltage", config.minVoltage);
  config.riseTime = preferences.getFloat("riseTime", config.riseTime);
  config.fallTime = preferences.getFloat("fallTime", config.fallTime);
  config.minHighTime = preferences.getFloat("minHighTime", config.minHighTime);
  config.maxHighTime = preferences.getFloat("maxHighTime", config.maxHighTime);
  config.minLowTime = preferences.getFloat("minLowTime", config.minLowTime);
  config.maxLowTime = preferences.getFloat("maxLowTime", config.maxLowTime);
  preferences.end();
  Serial.println("Config loaded from NVS");
  return true;
}

// Save config to SPIFFS
void saveConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  JsonDocument doc;
  doc["maxVoltage"] = config.maxVoltage;
  doc["minVoltage"] = config.minVoltage;
  doc["riseTime"] = config.riseTime;
  doc["fallTime"] = config.fallTime;
  doc["minHighTime"] = config.minHighTime;
  doc["maxHighTime"] = config.maxHighTime;
  doc["minLowTime"] = config.minLowTime;
  doc["maxLowTime"] = config.maxLowTime;

  serializeJson(doc, file);
  file.close();
  Serial.println("Config saved to SPIFFS");

  // Also save to NVS as backup
  saveConfigToNVS();
}

// Load config from SPIFFS
bool loadConfigFromSPIFFS() {
  if (!SPIFFS.exists(CONFIG_FILE)) {
    Serial.println("No config file found, using defaults");
    return false;
  }

  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }

  config.maxVoltage = doc["maxVoltage"] | config.maxVoltage;
  config.minVoltage = doc["minVoltage"] | config.minVoltage;
  config.riseTime = doc["riseTime"] | config.riseTime;
  config.fallTime = doc["fallTime"] | config.fallTime;
  config.minHighTime = doc["minHighTime"] | config.minHighTime;
  config.maxHighTime = doc["maxHighTime"] | config.maxHighTime;
  config.minLowTime = doc["minLowTime"] | config.minLowTime;
  config.maxLowTime = doc["maxLowTime"] | config.maxLowTime;

  Serial.println("Config loaded from SPIFFS");
  return true;
}

// Task handles
TaskHandle_t signalTask;

// Random with nonlinear distribution
float scaledRandom(float minVal, float maxVal) {
  float r = random(1000) / 1000.0;
  r = r * r;
  return minVal + (maxVal - minVal) * r;
}

// Signal generation task (runs on Core 0)
void signalLoop(void* parameter) {
  for (;;) {
    unsigned long currentMillis = millis();
    float progress;

    switch (state) {
      case 0:  // Low state
        outputVoltage = config.minVoltage;
        if (currentMillis - previousMillis >= lowTime * 1000 ||
            currentMillis - previousMillis >= (config.maxLowTime + config.safetyMargin) * 1000) {
          previousMillis = currentMillis;
          highTime = scaledRandom(config.minHighTime, config.maxHighTime);
          state = 1;
        }
        break;

      case 1:  // Rising state
        progress = (currentMillis - previousMillis) / (config.riseTime * 1000.0);
        if (progress >= 1.0) {
          progress = 1.0;
          previousMillis = currentMillis;
          state = 2;
        }
        outputVoltage = config.minVoltage + (config.maxVoltage - config.minVoltage) * sin(progress * (M_PI / 2));
        break;

      case 2:  // High state
        outputVoltage = config.maxVoltage;
        if (currentMillis - previousMillis >= highTime * 1000 ||
            currentMillis - previousMillis >= (config.maxHighTime + config.safetyMargin) * 1000) {
          previousMillis = currentMillis;
          lowTime = scaledRandom(config.minLowTime, config.maxLowTime);
          state = 3;
        }
        break;

      case 3:  // Falling state
        progress = (currentMillis - previousMillis) / (config.fallTime * 1000.0);
        if (progress >= 1.0) {
          progress = 1.0;
          previousMillis = currentMillis;
          state = 0;
        }
        outputVoltage = config.maxVoltage - (config.maxVoltage - config.minVoltage) * (1 - cos(progress * (M_PI / 2)));
        break;

      default:
        state = 0;
        outputVoltage = config.minVoltage;
        previousMillis = currentMillis;
        break;
    }

    // Output voltage using DAC (only if enabled)
    if (outputEnabled) {
      int dacValue = (outputVoltage / DAC_MAX_VOLTAGE) * 255;  // ESP32 DAC is 8-bit
      dacValue = constrain(dacValue, 0, 255);
      dacWrite(OUTPUT_PIN, dacValue);
    } else {
      dacWrite(OUTPUT_PIN, 0);  // Output disabled - hold at 0V
    }

    // Store in history buffer
    voltageHistory[historyIndex] = outputVoltage;
    stateHistory[historyIndex] = state;
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    sampleCounter++;

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void setupWebServer() {
  // Add CORS headers to all responses (allows IP-based API calls from mDNS-loaded page)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // Handle CORS preflight requests
  server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    request->send(204);
  });

  // Handle favicon request (return empty to avoid 500 error)
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(204);  // No content
  });

  // Serve static files from SPIFFS
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Get current config
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["maxVoltage"] = config.maxVoltage;
    doc["minVoltage"] = config.minVoltage;
    doc["riseTime"] = config.riseTime;
    doc["fallTime"] = config.fallTime;
    doc["minHighTime"] = config.minHighTime;
    doc["maxHighTime"] = config.maxHighTime;
    doc["minLowTime"] = config.minLowTime;
    doc["maxLowTime"] = config.maxLowTime;
    doc["outputEnabled"] = outputEnabled;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Toggle output on/off
  server.on("/toggle", HTTP_POST, [](AsyncWebServerRequest* request) {
    outputEnabled = !outputEnabled;
    Serial.printf("Output %s\n", outputEnabled ? "enabled" : "disabled");
    request->send(200, "application/json", outputEnabled ? "{\"enabled\":true}" : "{\"enabled\":false}");
  });

  // Get system status (WiFi, version, output state)
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["outputEnabled"] = outputEnabled;
    doc["wifiMode"] = WiFi.getMode() == WIFI_AP ? "AP" : "STA";
    doc["rssi"] = WiFi.getMode() == WIFI_STA ? WiFi.RSSI() : 0;
    doc["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    doc["uptime"] = millis() / 1000;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Export config as downloadable JSON
  server.on("/export", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["maxVoltage"] = config.maxVoltage;
    doc["minVoltage"] = config.minVoltage;
    doc["riseTime"] = config.riseTime;
    doc["fallTime"] = config.fallTime;
    doc["minHighTime"] = config.minHighTime;
    doc["maxHighTime"] = config.maxHighTime;
    doc["minLowTime"] = config.minLowTime;
    doc["maxLowTime"] = config.maxLowTime;
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    resp->addHeader("Content-Disposition", "attachment; filename=\"o2sim_config.json\"");
    request->send(resp);
  });

  // Import config from uploaded JSON
  server.on("/import", HTTP_POST, [](AsyncWebServerRequest* request) {},
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      config.maxVoltage = doc["maxVoltage"] | config.maxVoltage;
      config.minVoltage = doc["minVoltage"] | config.minVoltage;
      config.riseTime = doc["riseTime"] | config.riseTime;
      config.fallTime = doc["fallTime"] | config.fallTime;
      config.minHighTime = doc["minHighTime"] | config.minHighTime;
      config.maxHighTime = doc["maxHighTime"] | config.maxHighTime;
      config.minLowTime = doc["minLowTime"] | config.minLowTime;
      config.maxLowTime = doc["maxLowTime"] | config.maxLowTime;

      // Validate min <= max (auto-correct if needed)
      if (config.minVoltage > config.maxVoltage) {
        float temp = config.minVoltage;
        config.minVoltage = config.maxVoltage;
        config.maxVoltage = temp;
      }
      if (config.minHighTime > config.maxHighTime) {
        float temp = config.minHighTime;
        config.minHighTime = config.maxHighTime;
        config.maxHighTime = temp;
      }
      if (config.minLowTime > config.maxLowTime) {
        float temp = config.minLowTime;
        config.minLowTime = config.maxLowTime;
        config.maxLowTime = temp;
      }

      saveConfig();
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

  // Preset profiles
  server.on("/preset/normal", HTTP_POST, [](AsyncWebServerRequest* request) {
    config.maxVoltage = 0.8;
    config.minVoltage = 0.1;
    config.riseTime = 0.7;
    config.fallTime = 1.1;
    config.minHighTime = 1.25;
    config.maxHighTime = 10.0;
    config.minLowTime = 1.25;
    config.maxLowTime = 5.0;
    saveConfig();
    request->send(200, "application/json", "{\"status\":\"ok\",\"preset\":\"normal\"}");
  });

  server.on("/preset/aggressive", HTTP_POST, [](AsyncWebServerRequest* request) {
    config.maxVoltage = 0.9;
    config.minVoltage = 0.1;
    config.riseTime = 0.3;
    config.fallTime = 0.5;
    config.minHighTime = 0.5;
    config.maxHighTime = 3.0;
    config.minLowTime = 0.5;
    config.maxLowTime = 2.0;
    saveConfig();
    request->send(200, "application/json", "{\"status\":\"ok\",\"preset\":\"aggressive\"}");
  });

  server.on("/preset/slow", HTTP_POST, [](AsyncWebServerRequest* request) {
    config.maxVoltage = 0.7;
    config.minVoltage = 0.2;
    config.riseTime = 2.0;
    config.fallTime = 2.5;
    config.minHighTime = 3.0;
    config.maxHighTime = 15.0;
    config.minLowTime = 3.0;
    config.maxLowTime = 10.0;
    saveConfig();
    request->send(200, "application/json", "{\"status\":\"ok\",\"preset\":\"slow\"}");
  });

  // Update config
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest* request) {},
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      deserializeJson(doc, data, len);
      config.maxVoltage = doc["maxVoltage"] | config.maxVoltage;
      config.minVoltage = doc["minVoltage"] | config.minVoltage;
      config.riseTime = doc["riseTime"] | config.riseTime;
      config.fallTime = doc["fallTime"] | config.fallTime;
      config.minHighTime = doc["minHighTime"] | config.minHighTime;
      config.maxHighTime = doc["maxHighTime"] | config.maxHighTime;
      config.minLowTime = doc["minLowTime"] | config.minLowTime;
      config.maxLowTime = doc["maxLowTime"] | config.maxLowTime;

      // Validate min <= max (auto-correct if needed)
      if (config.minVoltage > config.maxVoltage) {
        float temp = config.minVoltage;
        config.minVoltage = config.maxVoltage;
        config.maxVoltage = temp;
      }
      if (config.minHighTime > config.maxHighTime) {
        float temp = config.minHighTime;
        config.minHighTime = config.maxHighTime;
        config.maxHighTime = temp;
      }
      if (config.minLowTime > config.maxLowTime) {
        float temp = config.minLowTime;
        config.minLowTime = config.maxLowTime;
        config.maxLowTime = temp;
      }

      saveConfig();  // Persist to SPIFFS
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

  // Get current voltage
  server.on("/voltage", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", String(outputVoltage, 2));
  });

  // Get voltage history buffer with sample counter for sync
  server.on("/history", HTTP_GET, [](AsyncWebServerRequest* request) {
    String response = "{\"counter\":";
    response += String(sampleCounter);
    response += ",\"data\":[";
    int idx = historyIndex;  // Start from oldest sample
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if (i > 0) response += ",";
      response += String(voltageHistory[idx], 2);
      idx = (idx + 1) % HISTORY_SIZE;
    }
    response += "],\"state\":[";
    idx = historyIndex;
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if (i > 0) response += ",";
      response += String(stateHistory[idx]);
      idx = (idx + 1) % HISTORY_SIZE;
    }
    response += "]}";
    request->send(200, "application/json", response);
  });

  // Setup OTA update endpoints
  setupOTA(server);

  server.begin();
}

void setup() {
  Serial.begin(115200);

  // Force 0V output at boot
  dacWrite(OUTPUT_PIN, 0);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  Serial.println("SPIFFS mounted");

  // Load saved config - try SPIFFS first, fall back to NVS
  if (!loadConfigFromSPIFFS()) {
    // SPIFFS config missing (e.g., after SPIFFS update), try NVS backup
    if (loadConfigFromNVS()) {
      // Restore SPIFFS config from NVS backup
      saveConfig();
      Serial.println("Config restored from NVS to SPIFFS");
    }
  }

  // Setup WiFi
  if (String(WIFI_MODE) == "AP+STA" || String(WIFI_MODE) == "STA") {
    // Connect to existing network (with optional AP)
    bool dualMode = String(WIFI_MODE) == "AP+STA";

    if (dualMode) {
      WiFi.mode(WIFI_AP_STA);
      // Start AP first
      if (strlen(AP_PASS) > 0) {
        WiFi.softAP(AP_SSID, AP_PASS);
      } else {
        WiFi.softAP(AP_SSID);
      }
      Serial.print("AP started: ");
      Serial.print(AP_SSID);
      Serial.print(" (");
      Serial.print(WiFi.softAPIP());
      Serial.println(")");
    } else {
      WiFi.mode(WIFI_STA);
    }

    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("Connecting to ");
    Serial.print(STA_SSID);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" Connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      // Start mDNS
      if (MDNS.begin(MDNS_NAME)) {
        Serial.print("mDNS: http://");
        Serial.print(MDNS_NAME);
        Serial.println(".local");
      }
    } else if (!dualMode) {
      // Fallback to AP mode if STA-only connection fails
      Serial.println(" Failed! Starting AP mode instead.");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID);
      Serial.print("Connect to: ");
      Serial.println(AP_SSID);
      Serial.print("IP: ");
      Serial.println(WiFi.softAPIP());
    } else {
      Serial.println(" Failed to connect to WiFi, but AP is still running.");
    }
  } else {
    // Access Point only mode
    WiFi.mode(WIFI_AP);
    if (strlen(AP_PASS) > 0) {
      WiFi.softAP(AP_SSID, AP_PASS);
    } else {
      WiFi.softAP(AP_SSID);  // Open network
    }
    Serial.println("WiFi AP started");
    Serial.print("Connect to: ");
    Serial.println(AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
  }

  // Setup web server
  setupWebServer();

  // Random seed (use micros + ESP MAC for entropy, avoid ADC2 which conflicts with WiFi)
  randomSeed(micros() ^ ESP.getEfuseMac());

  // Start signal generation on Core 0
  xTaskCreatePinnedToCore(
    signalLoop,
    "SignalTask",
    4096,
    NULL,
    1,
    &signalTask,
    0  // Core 0
  );

  // Initialize watchdog timer
  esp_task_wdt_init(WDT_TIMEOUT, true);  // true = reboot on timeout
  esp_task_wdt_add(NULL);  // Add current task (main loop) to watchdog

  Serial.println("O2 Simulator started");
}

void loop() {
  // Feed the watchdog - prevents reboot if loop is running
  esp_task_wdt_reset();

  // Check for pending OTA reboot
  handleReboot();

  // WiFi reconnect (only in STA mode)
  if (String(WIFI_MODE) == "STA" && millis() - lastWifiCheck > WIFI_CHECK_INTERVAL) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(STA_SSID, STA_PASS);
    }
  }

  // Periodic mDNS re-announcement (helps with client cache issues)
  if (String(WIFI_MODE) == "STA" && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastMdnsAnnounce > MDNS_ANNOUNCE_INTERVAL) {
      lastMdnsAnnounce = millis();
      MDNS.end();
      if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
      }
    }
  }

  // Main loop mostly empty - web server runs async, signal runs on Core 0
  vTaskDelay(100 / portTICK_PERIOD_MS);
}
