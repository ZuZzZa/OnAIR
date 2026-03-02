# Changelog

All notable changes to this project will be documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

**Versioning scheme:** `1.STEP.FIX`
- `STEP` — increments with each refactoring step
- `FIX`  — increments with each bug fix within a step (resets to 0 on new step)

---

## [1.3.0] — 2026-03-02

### Refactored
- Introduced `AP_SSID` and `AP_PASS` named constants at the top of the file,
  replacing all hardcoded string literals

### Fixed
- **AP config mode used two different SSIDs** — `setupAPMode()` broadcast
  `"OnAIR-Config"` while the WiFi fallback block inside `setupSTAMode()`
  broadcast `"ESP8266-Config"`, causing the device to appear under an
  unexpected network name when WiFi connection failed
- Both code paths now use `AP_SSID` / `AP_PASS`, so the AP name is
  consistent and can be changed in one place

---

## [1.2.0] — 2026-03-02

### Refactored
- Extracted `startAPServer()` helper — consolidates 5 AP route registrations,
  `server.begin()`, LED init, and startup log into one place
- `setupAPMode()` and the fallback block inside `setupSTAMode()` now both call
  `startAPServer()` instead of duplicating the same 12-line block

---

## [1.1.6] — 2026-03-02

### Refactored
- Collapsed five color-specific LED functions (`blinkRed`, `solidRed`,
  `blinkGreen`, `solidGreen`, `blinkBlue`) into two generic parameterized
  helpers: `ledBlink(mode, r, g, b)` and `ledSolid(mode, r, g, b)`
- Removed `solidGreen()` and `blinkGreen()` which were dead code (defined
  but never called)
- `updateLEDDisplay()` updated to use the new generic signatures
- Removed ~119 lines of duplicated code (1360 → 1241 lines)

---

## [1.1.5] — 2026-03-02

### Fixed
- **Log page showed no updates for hours** — `parseTeamsResponse()` was
  calling `addLog()` on every API poll (~every 3 s) regardless of state
  change, completely flooding the 8-entry circular buffer within ~12 seconds
  and overwriting any meaningful events
- `parseTeamsResponse()` now logs `Call:` / `Mute:` only when the state
  actually changes
- Increased `LOG_ENTRIES` from 8 to 32 to retain more history between
  state transitions

---

## [1.1.4] — 2026-03-02

### Added
- First stable release
- ESP8266 (WeMos D1 Mini Lite) firmware for OnAIR indicator
- Connects to WiFi in STA mode; falls back to AP config mode on failure
- Polls Microsoft Teams status via configurable HTTP API URL
- NeoPixel LED strip display:
  - Fast red blink — on call, mic live
  - Steady red — on call, muted
  - Blue blink — transient API error
  - Rainbow — error/pause state
  - LEDs off — idle / outside schedule
- Web UI at device IP: WiFi config, schedule, LED brightness, API URL
- OTA firmware update via `/update` endpoint
- Event log page at `/log` with auto-refresh every 3 s
- Weekly schedule with per-day enable/disable
- Configurable start/end hours with overnight wrap-around support
- NTP time sync (Ukrainian pool, EET/EEST timezone)
- Config stored in LittleFS as JSON
- Heap monitoring: logs free heap every 60 s
- Auto-increment build version via `increment_version.py` pre-build script

---

## [1.0.0] — 2026-03-01

### Added
- Initial repository setup (README, .gitignore)
