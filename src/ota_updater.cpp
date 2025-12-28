// ota_updater.cpp

#include "ota_updater.h"
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_task_wdt.h>

// DAC output pin (must match config.h)
#define OTA_OUTPUT_PIN 25

// Track if watchdog was disabled for OTA
static bool wdtDisabledForOTA = false;

bool shouldReboot = false;
unsigned long rebootTime = 0;

static bool invalidFile = false;
static bool otaError = false;
static String uploadedFilename;

void setupOTA(AsyncWebServer &server)
{
  Serial.println("[OTA] Initializing OTA updater endpoint");

  // Serve the firmware update page
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[OTA] Serving firmware update page");
    if (SPIFFS.exists("/firmware_update.html")) {
      request->send(SPIFFS, "/firmware_update.html", "text/html; charset=utf-8");
    } else {
      request->send(404, "text/plain", "Firmware update page not found");
    }
  });

  server.on("/update", HTTP_POST,

    // onRequest callback (after all chunks arrive)
    [](AsyncWebServerRequest *request)
    {
      shouldReboot = false;

      Serial.printf("[OTA] onRequest: file=\"%s\" invalidFile=%d otaError=%d\n",
                    uploadedFilename.c_str(), invalidFile, otaError);

      if (uploadedFilename.isEmpty()) {
        Serial.println("[OTA] Error: no file received");
        request->send(400, "text/plain", "INVALID_FILE");
      }
      else if (invalidFile) {
        Serial.printf("[OTA] Error: invalid filename '%s'\n", uploadedFilename.c_str());
        request->send(400, "text/plain", "INVALID_FILE");
      }
      else if (otaError) {
        Serial.printf("[OTA] Error: OTA failed for '%s'\n", uploadedFilename.c_str());
        request->send(500, "text/plain", "OTA_FAILED");
      }
      else {
        Serial.printf("[OTA] Success: '%s' updated, scheduling reboot in 3s\n", uploadedFilename.c_str());
        request->send(200, "text/plain", "OK");
        shouldReboot = true;
        rebootTime = millis() + 3000;
      }

      // Re-enable watchdog after OTA completes (success or failure)
      if (wdtDisabledForOTA) {
        esp_task_wdt_add(NULL);
        wdtDisabledForOTA = false;
        Serial.println("[OTA] Watchdog re-enabled");
      }

      uploadedFilename.clear();
      invalidFile = otaError = false;
    },

    // onUpload callback (per-chunk handler)
    [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      if (index == 0) {
        invalidFile = false;
        otaError = false;
        uploadedFilename = filename;

        // Disable watchdog during OTA to prevent timeout during long uploads
        esp_task_wdt_delete(NULL);
        wdtDisabledForOTA = true;
        Serial.println("[OTA] Watchdog disabled for update");

        // Force output to 0V for safety during update
        dacWrite(OTA_OUTPUT_PIN, 0);
        Serial.println("[OTA] Output disabled for safety");

        size_t total = req->contentLength();
        Serial.printf("[OTA] Upload start: %s (total %u bytes)\n", filename.c_str(), total);

        if (filename == "firmware.bin") {
          Serial.println("[OTA] Beginning firmware update...");
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            otaError = true;
            Serial.println("[OTA] Error: Update.begin(firmware) failed");
            return;
          }
        }
        else if (filename == "spiffs.bin") {
          Serial.println("[OTA] Beginning SPIFFS update...");
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            otaError = true;
            Serial.println("[OTA] Error: Update.begin(spiffs) failed");
            return;
          }
        }
        else {
          invalidFile = true;
          Serial.printf("[OTA] Error: filename '%s' not recognized\n", filename.c_str());
          return;
        }
      }

      if (!invalidFile && !otaError) {
        size_t written = Update.write(data, len);
        if (written != len) {
          otaError = true;
          Serial.printf("[OTA] Error: wrote %u of %u bytes\n", (unsigned)written, (unsigned)len);
        }
      }

      if (final) {
        Serial.println("[OTA] Finalizing update...");
        if (!invalidFile && !otaError) {
          if (Update.end(true)) {
            Serial.println("[OTA] Update.end(true) succeeded");
          } else {
            otaError = true;
            Serial.println("[OTA] Error: Update.end(true) failed");
          }
        }
      }
    }
  );
}

void handleReboot()
{
  if (shouldReboot && millis() >= rebootTime) {
    Serial.println("[OTA] Rebooting now...");
    ESP.restart();
  }
}
