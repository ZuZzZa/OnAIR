#include "api.h"

void enterErrorPause() {
  if (!isInErrorState) {
    isInErrorState   = true;
    lastErrorTime    = millis();
    isApiTransientError = false;
    addLog("Pausing 5 minutes before retry...");
  }
}

void incrementApiErrorCount() {
  apiErrorCount++;
  addLog("API errors: " + String(apiErrorCount) + "/5");
  if (apiErrorCount >= 5) {
    addLog("✗ 5 failed API connection attempts!");
    enterErrorPause();
  } else {
    isApiTransientError = true;
  }
}

void fetchAndUpdateLEDs() {
  HTTPClient http;

  Serial.print("[");
  Serial.print(millis());
  Serial.print("] Sending request to: ");
  Serial.println(config.apiUrl);

  http.begin(wifiClient, config.apiUrl);
  http.setTimeout(2000);

  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String payload = http.getString();
    Serial.print("✓ API response: ");
    Serial.println(payload);

    parseTeamsResponse(payload);

    retryCount          = 0;
    isInErrorState      = false;
    isApiTransientError = false;

    if (!apiEverSucceeded) addLog("✓ First successful API connection!");

    apiEverSucceeded = true;
    apiErrorCount    = 0;
    lastApiError     = "";

  } else if (httpResponseCode == 404) {
    addLog("✗ Error 404: invalid URL!");
    lastApiError = "404 Not Found - check API URL";
    if (apiEverSucceeded) {
      enterErrorPause();
    } else {
      incrementApiErrorCount();
    }

  } else {
    addLog("✗ HTTP error: " + String(httpResponseCode));
    Serial.println("✗ HTTP error: " + String(httpResponseCode));
    lastApiError = "HTTP " + String(httpResponseCode);
    if (apiEverSucceeded) {
      retryCount++;
      addLog("Retries: " + String(retryCount) + "/" + String(API_MAX_RETRIES));
      if (retryCount >= API_MAX_RETRIES) {
        addLog("Max retries reached, pausing 5 minutes...");
        retryCount = 0;
        enterErrorPause();
      } else {
        isApiTransientError = true;
      }
    } else {
      incrementApiErrorCount();
    }
  }

  http.end();
}

void parseTeamsResponse(String jsonResponse) {
  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (error) {
    addLog("✗ JSON parse error: " + String(error.f_str()));
    return;
  }

  if (doc.containsKey("call")) {
    String    val     = doc["call"].as<String>();
    CallState newCall = (val == "active") ? CALL_ACTIVE : CALL_INACTIVE;
    if (newCall != callState) {
      callState = newCall;
      addLog("Call: " + val);
    }
  }

  if (doc.containsKey("mute")) {
    String    val     = doc["mute"].as<String>();
    MuteState newMute = (val != "inactive") ? MIC_MUTED : MIC_LIVE;
    if (newMute != muteState) {
      muteState = newMute;
      addLog("Mute: " + val);
    }
  }

  Serial.println("LED state updated in memory");
}
