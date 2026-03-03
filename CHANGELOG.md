# Changelog

All notable changes to this project will be documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

**Versioning scheme:** `1.STEP.FIX`
- `STEP` — increments with each refactoring step
- `FIX`  — increments with each bug fix within a step (resets to 0 on new step)

---

## [1.8.0] — 2026-03-04

### Refactored
- Moved all NTP configuration out of `initializeNTP()` into named constants
  at the top of the file: `NTP_SERVER1/2/3`, `NTP_TIMEZONE`,
  `NTP_UTC_OFFSET`, `NTP_DST_OFFSET`, `NTP_SYNC_ATTEMPTS`, `NTP_ATTEMPT_MS`

### Added
- NTP sync retry logic — up to 3 attempts (configurable via `NTP_SYNC_ATTEMPTS`),
  each waiting up to 5 s (`NTP_ATTEMPT_MS`); log shows attempt number on
  success or failure (e.g. `✓ Time synced (attempt 1/3)`)

---

## [1.7.0] — 2026-03-04

### Refactored
- Replaced `String callState` / `String muteState` globals with typed enums:
  `CallState { CALL_INACTIVE, CALL_ACTIVE }` and
  `MuteState { MIC_LIVE, MIC_MUTED }`
- All heap-allocating `String` comparisons in the hot poll loop eliminated
- String↔enum conversion confined to two boundary points only:
  `parseTeamsResponse()` (API → enum) and `handleSTATUSJson()` (enum → JSON)
- All three enum types (`CallState`, `MuteState`, `LEDMode`) grouped under
  a dedicated `STATE ENUMS` section before the global variables

---

## [1.6.0] — 2026-03-02

### Refactored
- Extracted `enterErrorPause()` helper — sets `isInErrorState`, records timestamp,
  clears transient flag, and logs the 5-minute pause message; was duplicated 3×
- Extracted `incrementApiErrorCount()` helper — increments counter, logs progress,
  and calls `enterErrorPause()` at the threshold of 5; was duplicated 2×
- `fetchAndUpdateLEDs()` 404-branch and generic-HTTP-error branch now each reduce
  to 3–5 lines using the two helpers instead of 15–20 lines of inline logic

---

## [1.5.0] — 2026-03-02

### Refactored
- Extracted `applyJsonToConfig(doc, defaultApiUrl)` — unified the duplicated JSON
  field extraction (ssid, password, apiUrl, schedule, days) that existed in both
  `loadConfig()` and `handleSaveConfig()`
- `loadConfig()` calls `applyJsonToConfig(doc, "http://localhost:3491/v1/status")`
- `handleSaveConfig()` calls `applyJsonToConfig(doc)` (no default URL override)
- Also fixes the sign-compare compiler warnings in the days-array loop

---

## [1.4.0] — 2026-03-02

### Refactored
- Applied `F()` macro to all 209 static string literals in `handleConfigPage()`,
  `handleOTAPage()`, and `handleLogPage()` — moves string data from DRAM to flash
- Added `html.reserve()` to each handler (3200 / 1800 / 1200 bytes) to pre-allocate
  the String buffer and eliminate repeated heap reallocations during page builds
- **RAM usage reduced from 54.2% → 43.9%** (saved 8,408 bytes of DRAM at startup)

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

---

## [2.0.0] — 2026-03-04

### Refactored
- **Split monolithic `main.cpp` (1234 lines) into logical modules** — step 9
  of the refactoring plan; `main.cpp` now contains only `setup()` + `loop()`
  (117 lines)

**New source files:**

| File | Responsibility |
|---|---|
| `src/globals.h` | All `#define` settings, enums, struct types, `extern` declarations, `addLog()` declaration |
| `src/globals.cpp` | Shared variable definitions + `addLog()` implementation |
| `src/config.h/.cpp` | `loadConfig()`, `saveConfig()`, `applyJsonToConfig()` |
| `src/led.h/.cpp` | LED primitives, `updateLEDDisplay()`, `isScheduleActive()` |
| `src/wifi_manager.h/.cpp` | AP/STA setup, `connectToWiFi()`, `initializeNTP()`, `printCurrentTime()` |
| `src/api.h/.cpp` | `fetchAndUpdateLEDs()`, `parseTeamsResponse()`, error helpers |
| `src/web_handlers.h/.cpp` | All HTTP route handlers (config, save, status, OTA, log) |
| `src/main.cpp` | `setup()` + `loop()` only |

### Changed
- `apiRequestInterval` / `apiRequestTimeout` / `maxRetries` constants renamed to
  `API_REQUEST_INTERVAL_MS` / `API_REQUEST_TIMEOUT_MS` / `API_MAX_RETRIES`
  (`#define` instead of `const` to avoid ODR issues across translation units)
- Version bumped to **2.0.0** — major bump reflects the architectural change
  from single-file to multi-module project layout

