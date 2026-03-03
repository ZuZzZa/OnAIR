#include "globals.h"
#include "config.h"
#include "led.h"
#include "wifi_manager.h"
#include "api.h"
#include "web_handlers.h"

// ============== SETUP ==============

void setup() {
  Serial.begin(115200);
  delay(100);

  // Write STATION_MODE to the SDK's persistent flash sectors once so that
  // on every boot the SDK initialises in STA mode.  A stale AP/AP_STA
  // opmode left by a previous firmware will exhaust the internal ESF buffer
  // pool before setup() runs, causing a crash on any WiFi.mode() call.
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  delay(100);

  addLog("=== FIRMWARE v" FIRMWARE_VERSION " INIT ===");

  if (!LittleFS.begin()) {
    addLog("✗ LittleFS init failed");
  } else {
    addLog("✓ LittleFS initialized");
  }

  strip.begin();
  strip.setBrightness(BRIGHTNESS);

  for (int retry = 0; retry < 3; retry++) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(10);
  }

  loadConfig();
  delay(100);

  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  }
  strip.show();
  delay(500);

  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();

  if (strlen(config.ssid) > 0 && strlen(config.password) > 0) {
    addLog("Saved WiFi config found");
    setupSTAMode();
  } else {
    addLog("No config found, starting AP mode");
    setupAPMode();
  }

  addLog("=== INIT COMPLETE ===");
}

// ============== MAIN LOOP ==============

void loop() {
  server.handleClient();

  if (isOTAInProgress) {
    yield();
    return;
  }

  if (currentMode == MODE_STA_NORMAL) {
    if (WiFi.status() != WL_CONNECTED) {
      addLog("WiFi disconnected, reconnecting...");
      connectToWiFi();
    }

    if (isInErrorState) {
      if (millis() - lastErrorTime >= API_REQUEST_TIMEOUT_MS) {
        addLog("Pause ended!");
        isInErrorState      = false;
        retryCount          = 0;
        apiErrorCount       = 0;
        isApiTransientError = false;

        if (isScheduleActive()) {
          addLog("Within schedule, starting new attempt...");
          lastApiRequest = millis();
          fetchAndUpdateLEDs();
        } else {
          addLog("Outside schedule, attempt deferred");
        }
      }
    } else {
      if (isScheduleActive() && (millis() - lastApiRequest >= API_REQUEST_INTERVAL_MS)) {
        lastApiRequest = millis();
        fetchAndUpdateLEDs();
      }
    }

    updateLEDDisplay();

    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog >= 60000) {
      lastHeapLog = millis();
      addLog("[HEAP] Free: " + String(ESP.getFreeHeap()) + " bytes");
    }
  }
}
