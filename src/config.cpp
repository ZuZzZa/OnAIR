#include "config.h"

void applyJsonToConfig(JsonDocument& doc, const char* defaultApiUrl) {
  strlcpy(config.ssid,     doc["ssid"]     | "",            sizeof(config.ssid));
  strlcpy(config.password, doc["password"] | "",            sizeof(config.password));
  strlcpy(config.apiUrl,   doc["apiUrl"]   | defaultApiUrl, sizeof(config.apiUrl));

  if (doc.containsKey("schedule")) {
    schedule.enabled   = doc["schedule"]["enabled"]   | true;
    schedule.startHour = doc["schedule"]["startHour"] | 0;
    schedule.endHour   = doc["schedule"]["endHour"]   | 23;

    if (doc["schedule"].containsKey("days")) {
      JsonArray daysArray = doc["schedule"]["days"];
      for (int i = 0; i < 7 && i < (int)daysArray.size(); i++) {
        schedule.days[i] = daysArray[i] | true;
      }
    }
  }
}

void loadConfig() {
  memset(&config, 0, sizeof(config));
  memset(&schedule, 0, sizeof(schedule));

  schedule.enabled   = true;
  schedule.startHour = 0;
  schedule.endHour   = 23;
  for (int i = 0; i < 7; i++) schedule.days[i] = true;

  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    if (file) {
      StaticJsonDocument<768> doc;
      if (deserializeJson(doc, file) == DeserializationError::Ok) {
        applyJsonToConfig(doc, "http://localhost:3491/v1/status");
        addLog("✓ Config loaded: SSID=" + String(config.ssid));
      }
      file.close();
    }
  } else {
    strcpy(config.apiUrl, "http://localhost:3491/v1/status");
    Serial.println("Using default settings");
  }
}

void saveConfig() {
  StaticJsonDocument<768> doc;
  doc["ssid"]     = config.ssid;
  doc["password"] = config.password;
  doc["apiUrl"]   = config.apiUrl;

  doc["schedule"]["enabled"]   = schedule.enabled;
  doc["schedule"]["startHour"] = schedule.startHour;
  doc["schedule"]["endHour"]   = schedule.endHour;

  JsonArray daysArray = doc["schedule"].createNestedArray("days");
  for (int i = 0; i < 7; i++) daysArray.add(schedule.days[i]);

  File file = LittleFS.open("/config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    addLog("✓ Config saved");
  } else {
    addLog("✗ Failed to save config");
  }
}
