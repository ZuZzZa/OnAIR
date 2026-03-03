#pragma once

#include <Arduino.h>
#include "version.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

// ============== SETTINGS ==============

#define MODE_UNKNOWN    0
#define MODE_AP_CONFIG  1
#define MODE_STA_NORMAL 2

#define LED_PIN    D4
#define NUM_LEDS   10
#define BRIGHTNESS 150

#define OTA_PASSWORD "onair_ota"

#define AP_SSID "OnAIR-Config"
#define AP_PASS "12345678"

#define LOG_ENTRIES 32
#define LOG_MAX_LEN 64

#define API_REQUEST_INTERVAL_MS 3000UL
#define API_REQUEST_TIMEOUT_MS  300000UL
#define API_MAX_RETRIES         3

#define NTP_SERVER1       "0.ua.pool.ntp.org"
#define NTP_SERVER2       "1.ua.pool.ntp.org"
#define NTP_SERVER3       "2.ua.pool.ntp.org"
#define NTP_TIMEZONE      "EET-2EEST,M3.5.0/3,M10.5.0/4"
#define NTP_UTC_OFFSET    (2 * 3600)
#define NTP_DST_OFFSET    (1 * 3600)
#define NTP_SYNC_ATTEMPTS 3
#define NTP_ATTEMPT_MS    5000

// ============== STATE ENUMS ==============

enum CallState { CALL_INACTIVE, CALL_ACTIVE };
enum MuteState { MIC_LIVE, MIC_MUTED };
enum LEDMode   { LED_OFF, LED_RED_BLINK, LED_RED_SOLID, LED_GREEN_BLINK, LED_GREEN_SOLID, LED_BLUE_BLINK, LED_RAINBOW };

// ============== STRUCT TYPES ==============

struct Schedule {
  bool    enabled;
  uint8_t startHour;
  uint8_t endHour;
  bool    days[7];
};

struct Config {
  char ssid[32];
  char password[64];
  char apiUrl[128];
};

struct LogEntry {
  unsigned long ts;
  char          text[LOG_MAX_LEN];
};

// ============== SHARED OBJECTS ==============

extern Adafruit_NeoPixel strip;
extern ESP8266WebServer  server;
extern WiFiClient        wifiClient;

// ============== SHARED STATE ==============

extern Config   config;
extern Schedule schedule;

extern int           currentMode;
extern CallState     callState;
extern MuteState     muteState;
extern LEDMode       lastLEDMode;
extern LEDMode       currentLEDMode;

extern unsigned long lastApiRequest;
extern unsigned long lastErrorTime;
extern int           retryCount;
extern bool          isInErrorState;

extern String        lastError;
extern int           wifiConnectAttempts;

extern bool          apiEverSucceeded;
extern int           apiErrorCount;
extern String        lastApiError;
extern unsigned long lastApiErrorTime;

extern bool          isApiTransientError;
extern bool          isOTAInProgress;

extern LogEntry      eventLog[LOG_ENTRIES];
extern int           logHead;
extern int           logCount;

// ============== EVENT LOG ==============

void addLog(const String& msg);
