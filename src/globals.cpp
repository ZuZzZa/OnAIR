#include "globals.h"

// ============== SHARED OBJECTS ==============

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer  server(80);
WiFiClient        wifiClient;

// ============== SHARED STATE ==============

Config   config;
Schedule schedule;

int      currentMode   = MODE_UNKNOWN;
CallState callState    = CALL_INACTIVE;
MuteState muteState    = MIC_LIVE;
LEDMode  lastLEDMode   = LED_OFF;
LEDMode  currentLEDMode = LED_OFF;

unsigned long lastApiRequest  = 0;
unsigned long lastErrorTime   = 0;
int           retryCount      = 0;
bool          isInErrorState  = false;

String lastError          = "";
int    wifiConnectAttempts = 0;

bool          apiEverSucceeded = false;
int           apiErrorCount    = 0;
String        lastApiError     = "";
unsigned long lastApiErrorTime = 0;

bool isApiTransientError = false;
bool isOTAInProgress     = false;

LogEntry eventLog[LOG_ENTRIES];
int      logHead  = 0;
int      logCount = 0;

// ============== EVENT LOG ==============

void addLog(const String& msg) {
  Serial.println(msg);
  eventLog[logHead].ts = millis();
  strncpy(eventLog[logHead].text, msg.c_str(), LOG_MAX_LEN - 1);
  eventLog[logHead].text[LOG_MAX_LEN - 1] = '\0';
  logHead = (logHead + 1) % LOG_ENTRIES;
  if (logCount < LOG_ENTRIES) logCount++;
}
