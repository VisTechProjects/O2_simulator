#pragma once
#include <ESPAsyncWebServer.h>

extern bool shouldReboot;
extern unsigned long rebootTime;

void setupOTA(AsyncWebServer &server);
void handleReboot();
