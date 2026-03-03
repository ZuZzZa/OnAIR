#include "wifi_manager.h"
#include "led.h"
#include "web_handlers.h"

void startAPServer() {
  setAllLEDs(255, 255, 0);

  server.on("/",        handleConfigPage);
  server.on("/save",    handleSaveConfig);
  server.on("/status",  handleSTATUSJson);
  server.on("/log",     handleLogPage);
  server.on("/logdata", handleLogData);
  server.begin();

  addLog("✓ Web server started at http://192.168.4.1");
}

void setupAPMode() {
  currentMode = MODE_AP_CONFIG;

  addLog("=== ACCESS POINT MODE ===");
  Serial.print("SSID: " AP_SSID " | IP: 192.168.4.1");
  Serial.println("\n");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  startAPServer();
}

void setupSTAMode() {
  currentMode        = MODE_STA_NORMAL;
  wifiConnectAttempts = 0;

  apiEverSucceeded = false;
  apiErrorCount    = 0;
  lastApiError     = "";

  addLog("=== STA MODE ===");

  for (int attempt = 1; attempt <= 3; attempt++) {
    addLog("Connection attempt #" + String(attempt) + " to WiFi: " + String(config.ssid));

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
      if (attempts % 2 == 0) setAllLEDs(255, 255, 0);
      else                    setAllLEDs(0, 0, 0);
    }

    if (WiFi.status() == WL_CONNECTED) {
      addLog("✓ WiFi connected!");
      addLog("IP address: " + WiFi.localIP().toString());
      addLog("API URL: " + String(config.apiUrl));

      lastError = "";

      initializeNTP();
      printCurrentTime();

      server.on("/",       handleConfigPage);
      server.on("/save",   handleSaveConfig);
      server.on("/status", handleSTATUSJson);
      server.on("/update", HTTP_GET,  handleOTAPage);
      server.on("/update", HTTP_POST, handleOTAUploadComplete, handleOTAFileUpload);
      server.on("/log",     handleLogPage);
      server.on("/logdata", handleLogData);
      server.begin();
      addLog("✓ Web server started at http://" + WiFi.localIP().toString());

      setAllLEDs(255, 255, 0);
      delay(1000);
      setAllLEDs(0, 0, 0);
      return;
    } else {
      addLog("✗ Attempt #" + String(attempt) + " failed - WiFi error");
      lastError = "WiFi connection failed";
      if (attempt < 3) delay(2000);
    }
  }

  addLog("✗ All 3 connection attempts failed!");
  addLog("Starting config mode...");

  currentMode = MODE_AP_CONFIG;
  wifiConnectAttempts = 3;

  WiFi.disconnect();
  delay(200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  startAPServer();
  addLog("✓ SSID: " + String(AP_SSID) + " | Password: " + String(AP_PASS));
}

void connectToWiFi() {
  if (WiFi.isConnected()) return;

  WiFi.reconnect();
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addLog("Reconnected successfully!");
  } else {
    addLog("Reconnection failed");
  }
}

void initializeNTP() {
  addLog("=== TIME SYNC ===");

  setenv("TZ", NTP_TIMEZONE, 1);
  tzset();

  for (int attempt = 1; attempt <= NTP_SYNC_ATTEMPTS; attempt++) {
    Serial.printf("NTP attempt %d/%d...\n", attempt, NTP_SYNC_ATTEMPTS);
    configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

    unsigned long start = millis();
    time_t now = time(nullptr);
    while (now < 24 * 3600 && millis() - start < NTP_ATTEMPT_MS) {
      delay(100);
      now = time(nullptr);
    }

    if (now > 24 * 3600) {
      addLog("✓ Time synced (attempt " + String(attempt) + "/" + String(NTP_SYNC_ATTEMPTS) + ")");
      return;
    }

    addLog("✗ NTP attempt " + String(attempt) + "/" + String(NTP_SYNC_ATTEMPTS) + " failed");
    if (attempt < NTP_SYNC_ATTEMPTS) delay(1000);
  }

  addLog("✗ Time sync failed after " + String(NTP_SYNC_ATTEMPTS) + " attempts");
}

void printCurrentTime() {
  time_t    now      = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  char timeString[64];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S %Z", timeinfo);

  Serial.println("\n=== CURRENT TIME ===");
  Serial.print("Local time (Kyiv): ");
  Serial.println(timeString);

  const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  Serial.print("Day of week: ");
  Serial.println(days[timeinfo->tm_wday == 0 ? 6 : timeinfo->tm_wday - 1]);
  Serial.println("========================\n");
}
