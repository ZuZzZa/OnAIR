# OnAIR

A compact Wi-Fi connected indicator light that shows your real-time call status on a NeoPixel LED strip. When you're on a call it lights up — so anyone walking past your desk knows not to interrupt.

---

## About the Project

OnAIR runs on a **WeMos D1 Mini Lite** (ESP8266) and polls a local HTTP API that exposes your current meeting status (Microsoft Teams, Google Meet, Zoom, or any source you expose via the API). The LED strip changes colour and pattern instantly when your call or mute state changes.

The device hosts its own web interface for configuration — no app or cloud account required. Everything is stored locally on the device in LittleFS.

**Hardware:**
| Component | Details |
|---|---|
| MCU | WeMos D1 Mini Lite (ESP8266, 80 MHz, 1 MB flash) |
| LED strip | NeoPixel-compatible WS2812B, 10 LEDs, pin D4 |
| Power | USB (5 V via the D1 Mini onboard regulator) |

---

## Features

### LED Status Display
| State | LED Behaviour |
|---|---|
| On call, mic live | Fast red blink (250 ms) |
| On call, muted | Steady red |
| Transient API error | Blue blink |
| Persistent error / pause | Rainbow cycle |
| Idle / outside schedule | LEDs off |

### Wi-Fi & Connectivity
- Connects to your network in **STA mode** on boot
- Falls back to **AP config mode** (`OnAIR-Config` / `12345678`) if connection fails
- Automatically reconnects on drop
- Up to 3 connection attempts before falling back to AP mode

### Web Interface (served from device IP)
- **`/`** — Wi-Fi credentials, API URL, LED brightness, weekly schedule
- **`/log`** — Live event log, auto-refreshes every 3 s
- **`/update`** — OTA firmware update via `.bin` file upload
- **`/status`** — JSON endpoint with current call/mute state and device info

### Weekly Schedule
- Enable/disable per day of the week (Mon–Sun)
- Configurable start and end hour (supports overnight wrap-around)
- LEDs turn off automatically outside the active window

### OTA Firmware Update
- Upload a compiled `.bin` directly from the browser at `/update`
- LED strip shows a white progress bar during upload
- Device reboots automatically on success

### Event Log
- Circular buffer of the last 32 events
- Logs state changes, errors, Wi-Fi events, heap stats, and boot sequence
- Heap usage logged every 60 s for memory monitoring

### Time Sync
- NTP sync on boot via Ukrainian pool (`*.ua.pool.ntp.org`)
- Timezone: EET/EEST (UTC+2/UTC+3, Kyiv)

### Build Versioning
- Build number auto-incremented before every PlatformIO compile via `increment_version.py`
- Version string embedded in firmware and visible in the event log on boot

---

## Libraries

| Library | Version | Purpose |
|---|---|---|
| [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) | `^1.11.3` | WS2812B LED strip control |
| [ArduinoJson](https://arduinojson.org) | `^6.21.3` | JSON parsing for API responses and LittleFS config |
| [ESP8266WiFi](https://github.com/esp8266/Arduino) | bundled | Wi-Fi STA/AP management |
| [ESP8266HTTPClient](https://github.com/esp8266/Arduino) | bundled | HTTP GET requests to the status API |
| [ESP8266WebServer](https://github.com/esp8266/Arduino) | bundled | Web UI and OTA endpoint |
| [LittleFS](https://github.com/esp8266/Arduino) | bundled | Persistent config storage on flash |
| [time.h / NTP](https://github.com/esp8266/Arduino) | bundled | NTP time sync for schedule evaluation |

---

## Versioning

`MAJOR.STEP.FIX`
- `STEP` — increments with each completed refactoring step
- `FIX` — increments with each bug fix within a step, resets to 0 on new step

See [CHANGELOG.md](CHANGELOG.md) for full history.


---

## Project Structure

After the v2.0.0 refactoring, the firmware source is split into focused modules. Each module owns a single area of responsibility and exposes a clean `.h` interface.

```
src/
├── globals.h          # All #defines, enums, struct types, extern declarations, addLog()
├── globals.cpp        # Shared variable definitions + addLog() implementation
│
├── config.h           # Config module interface
├── config.cpp         # loadConfig(), saveConfig(), applyJsonToConfig()
│
├── led.h              # LED module interface
├── led.cpp            # LED primitives, updateLEDDisplay(), isScheduleActive()
│
├── wifi_manager.h     # WiFi module interface
├── wifi_manager.cpp   # setupAPMode(), setupSTAMode(), connectToWiFi(), NTP
│
├── api.h              # API module interface
├── api.cpp            # fetchAndUpdateLEDs(), parseTeamsResponse(), error helpers
│
├── web_handlers.h     # Web handlers interface
├── web_handlers.cpp   # HTTP route handlers: config, save, status, OTA, log
│
├── main.cpp           # setup() + loop() only (~117 lines)
└── version.h          # Auto-generated version constants (FIRMWARE_VERSION)
```

### Module responsibilities

| Module | Key symbols |
|---|---|
| `globals` | `Config`, `Schedule`, `LogEntry`, `CallState`, `MuteState`, `LEDMode`, all shared globals, `addLog()` |
| `config` | `loadConfig()`, `saveConfig()`, `applyJsonToConfig()` — LittleFS JSON persistence |
| `led` | `ledBlink()`, `ledSolid()`, `showRainbow()`, `updateLEDDisplay()`, `isScheduleActive()` |
| `wifi_manager` | `setupAPMode()`, `setupSTAMode()`, `connectToWiFi()`, `initializeNTP()` |
| `api` | `fetchAndUpdateLEDs()`, `parseTeamsResponse()`, `enterErrorPause()`, `incrementApiErrorCount()` |
| `web_handlers` | `handleConfigPage()`, `handleSaveConfig()`, `handleSTATUSJson()`, `handleOTAPage()`, `handleLogPage()`, `handleLogData()` |
