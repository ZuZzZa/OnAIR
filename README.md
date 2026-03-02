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

## Configuration

Key constants in `src/main.cpp`:

```cpp
#define LED_PIN          D4           // NeoPixel data pin
#define NUM_LEDS         10           // Number of LEDs in the strip
#define BRIGHTNESS       150          // 0–255
#define AP_SSID          "OnAIR-Config"
#define AP_PASS          "12345678"
#define OTA_PASSWORD     "onair_ota"
#define LOG_ENTRIES      32           // Circular event log size

const unsigned long apiRequestInterval = 3000;   // ms between API polls
const unsigned long apiRequestTimeout  = 300000; // ms pause after max retries
const int           maxRetries         = 3;
```

---

## Versioning

`MAJOR.STEP.FIX`
- `STEP` — increments with each completed refactoring step
- `FIX` — increments with each bug fix within a step, resets to 0 on new step

See [CHANGELOG.md](CHANGELOG.md) for full history.
