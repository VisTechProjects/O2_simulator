# O2 Sensor Simulator - ESP32 Web Edition

## Description
Deleted your catalytic converter for "off road use"? Have a P0420/P0430 check engine code? This project simulates downstream O2 sensor readings using an ESP32 with a web-based control interface. Works for single or multiple sensors (connect all sensors to simulator output).

![Web Interface](/page_preview.png)

## Features
- **Web Interface** - Control and monitor via any browser (phone, tablet, laptop)
- **Live Voltage Graph** - Real-time waveform display with color-coded states
- **Adjustable Parameters** - Voltage, rise/fall times, hold times all configurable
- **Presets** - Normal, Aggressive, and Slow profiles
- **Config Persistence** - Settings saved to flash, survive power cycles
- **OTA Updates** - Update firmware wirelessly via web browser
- **mDNS Support** - Access via `http://o2sim.local`
- **Output Toggle** - Enable/disable output with one tap
- **Export/Import** - Backup and restore configurations as JSON

## Hardware
- **ESP32** dev board (any variant with DAC)
- **Output Pin**: GPIO25 (DAC1)
- Connect output to downstream O2 sensor signal wire

## Quick Start

1. **Copy config template:**
   ```bash
   cp src/config.h.example src/config.h
   ```

2. **Edit `src/config.h`** with your WiFi credentials:
   ```cpp
   const char* STA_SSID = "YourWiFiSSID";
   const char* STA_PASS = "YourWiFiPassword";
   ```

3. **Build and upload** (PlatformIO):
   ```bash
   pio run --target upload
   pio run --target uploadfs
   ```

4. **Access the web interface:**
   - Via mDNS: `http://o2sim.local`
   - Or check serial monitor for IP address

## WiFi Modes

**Station Mode (default)** - Connects to your existing WiFi network

**AP Mode** - Creates its own hotspot. Edit `config.h`:
```cpp
#define WIFI_MODE "AP"
```
Then connect to `O2_Simulator` network and go to `192.168.4.1`

## Default Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Max Voltage | 0.8V | Rich signal (typical 0.7-0.9V) |
| Min Voltage | 0.0V | Lean signal (typical 0.1-0.2V) |
| Rise Time | 0.7s | Transition low to high |
| Fall Time | 1.1s | Transition high to low |
| Min High Time | 1.25s | Minimum hold at high voltage |
| Max High Time | 10s | Maximum hold at high voltage |
| Min Low Time | 1.25s | Minimum hold at low voltage |
| Max Low Time | 5s | Maximum hold at low voltage |

## OTA Firmware Update

1. Go to `http://o2sim.local/firmware`
2. Drag and drop `firmware.bin` or `spiffs.bin`
3. Wait for upload and automatic reboot

## Tips
- Double-click the live graph to toggle between colored and solid green trace
- Use AP mode if you need the simulator to work without an existing network
- Export your config before updating SPIFFS to preserve settings

## Original Signal Output
![O2 simulator output](/o2_output.jpg)
