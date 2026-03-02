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

#define MODE_UNKNOWN 0
#define MODE_AP_CONFIG 1
#define MODE_STA_NORMAL 2

#define LED_PIN D4
#define NUM_LEDS 10
#define BRIGHTNESS 150

#define OTA_PASSWORD "onair_ota"   // used by web OTA (/update) — browser must enter this

#define LOG_ENTRIES 32
#define LOG_MAX_LEN 64

const unsigned long apiRequestInterval = 3000;
const unsigned long apiRequestTimeout = 300000;
const int maxRetries = 3;

// ============== GLOBAL VARIABLES ==============

struct Schedule {
  bool enabled;
  uint8_t startHour;
  uint8_t endHour;
  bool days[7];
} schedule;

struct Config {
  char ssid[32];
  char password[64];
  char apiUrl[128];
} config;

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);
unsigned long lastApiRequest = 0;
unsigned long lastErrorTime = 0;
int retryCount = 0;
bool isInErrorState = false;
WiFiClient wifiClient;
int currentMode = MODE_UNKNOWN;

String callState = "inactive";
String muteState = "inactive";

String lastError = "";
int wifiConnectAttempts = 0;

bool apiEverSucceeded = false;
int apiErrorCount = 0;
String lastApiError = "";
unsigned long lastApiErrorTime = 0;

bool isApiTransientError = false;
bool isOTAInProgress = false;

struct LogEntry { unsigned long ts; char text[LOG_MAX_LEN]; };
LogEntry eventLog[LOG_ENTRIES];
int logHead = 0;
int logCount = 0;

// LED mode tracking for resetting statics on mode change
enum LEDMode { LED_OFF, LED_RED_BLINK, LED_RED_SOLID, LED_GREEN_BLINK, LED_GREEN_SOLID, LED_BLUE_BLINK, LED_RAINBOW };
LEDMode lastLEDMode = LED_OFF;
LEDMode currentLEDMode = LED_OFF;

// ============== FUNCTION DECLARATIONS ==============

void loadConfig();
void saveConfig();
void setupAPMode();
void setupSTAMode();
void handleConfigPage();
void handleSaveConfig();
void handleSTATUSJson();
bool isScheduleActive();
void setupWiFi();
void initializeNTP();
void printCurrentTime();
void connectToWiFi();
void fetchAndUpdateLEDs();
void parseTeamsResponse(String jsonResponse);
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
void setLEDColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void clearAllLEDs();
void forceClearBuffer();
void ledBlink(LEDMode mode, uint8_t r, uint8_t g, uint8_t b);
void ledSolid(LEDMode mode, uint8_t r, uint8_t g, uint8_t b);
void showRainbow();
void updateLEDDisplay();
void handleOTAPage();
void handleOTAUploadComplete();
void handleOTAFileUpload();
void addLog(const String& msg);
void handleLogPage();
void handleLogData();

// ============== EVENT LOG ==============

void addLog(const String& msg) {
  Serial.println(msg);
  eventLog[logHead].ts = millis();
  strncpy(eventLog[logHead].text, msg.c_str(), LOG_MAX_LEN - 1);
  eventLog[logHead].text[LOG_MAX_LEN - 1] = '\0';
  logHead = (logHead + 1) % LOG_ENTRIES;
  if (logCount < LOG_ENTRIES) logCount++;
}

// ============== SETUP ==============

void setup() {
  Serial.begin(115200);
  delay(100);

  // Write STATION_MODE to the SDK's persistent flash sectors once so that
  // on every boot the SDK initialises in STA mode.  A stale AP/AP_STA
  // opmode left by a previous firmware will exhaust the internal ESF buffer
  // pool before setup() runs, causing a crash on any WiFi.mode() call.
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);       // save STATION_MODE to flash
  WiFi.persistent(false);    // all subsequent mode changes are non-persistent
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
      if (millis() - lastErrorTime >= apiRequestTimeout) {
        addLog("Pause ended!");
        isInErrorState = false;
        retryCount = 0;
        apiErrorCount = 0;
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
      if (isScheduleActive() && (millis() - lastApiRequest >= apiRequestInterval)) {
        lastApiRequest = millis();
        fetchAndUpdateLEDs();
      }
    }
    
    updateLEDDisplay();
    
    // Heap monitoring: log free heap every 60 seconds
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog >= 60000) {
      lastHeapLog = millis();
      addLog("[HEAP] Free: " + String(ESP.getFreeHeap()) + " bytes");
    }
  }
  
  yield();
}

// ============== CONFIG FUNCTIONS ==============

void loadConfig() {
  memset(&config, 0, sizeof(config));
  memset(&schedule, 0, sizeof(schedule));
  
  schedule.enabled = true;
  schedule.startHour = 0;
  schedule.endHour = 23;
  for (int i = 0; i < 7; i++) {
    schedule.days[i] = true;
  }
  
  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    if (file) {
      StaticJsonDocument<768> doc;
      if (deserializeJson(doc, file) == DeserializationError::Ok) {
        strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
        strlcpy(config.password, doc["password"] | "", sizeof(config.password));
        strlcpy(config.apiUrl, doc["apiUrl"] | "http://localhost:3491/v1/status", sizeof(config.apiUrl));
        
        if (doc.containsKey("schedule")) {
          schedule.enabled = doc["schedule"]["enabled"] | true;
          schedule.startHour = doc["schedule"]["startHour"] | 0;
          schedule.endHour = doc["schedule"]["endHour"] | 23;
          
          JsonArray daysArray = doc["schedule"]["days"];
          for (int i = 0; i < 7 && i < daysArray.size(); i++) {
            schedule.days[i] = daysArray[i] | true;
          }
        }
        
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
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["apiUrl"] = config.apiUrl;
  
  doc["schedule"]["enabled"] = schedule.enabled;
  doc["schedule"]["startHour"] = schedule.startHour;
  doc["schedule"]["endHour"] = schedule.endHour;
  
  JsonArray daysArray = doc["schedule"].createNestedArray("days");
  for (int i = 0; i < 7; i++) {
    daysArray.add(schedule.days[i]);
  }
  
  File file = LittleFS.open("/config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    addLog("✓ Config saved");
  } else {
    addLog("✗ Failed to save config");
  }
}

// ============== OPERATING MODES ==============

void setupAPMode() {
  currentMode = MODE_AP_CONFIG;
  
  addLog("=== ACCESS POINT MODE ===");
  Serial.print("SSID: OnAIR-Config | IP: 192.168.4.1");
  Serial.println("\n");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("OnAIR-Config", "12345678");
  
  setAllLEDs(255, 255, 0);
  
  server.on("/", handleConfigPage);
  server.on("/save", handleSaveConfig);
  server.on("/status", handleSTATUSJson);
  server.on("/log", handleLogPage);
  server.on("/logdata", handleLogData);
  server.begin();
  
  addLog("✓ Web server started at http://192.168.4.1");
}

void setupSTAMode() {
  currentMode = MODE_STA_NORMAL;
  wifiConnectAttempts = 0;
  
  apiEverSucceeded = false;
  apiErrorCount = 0;
  lastApiError = "";
  
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
      
      if (attempts % 2 == 0) {
        setAllLEDs(255, 255, 0);
      } else {
        setAllLEDs(0, 0, 0);
      }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      addLog("✓ WiFi connected!");
      addLog("IP address: " + WiFi.localIP().toString());
      addLog("API URL: " + String(config.apiUrl));
      
      lastError = "";
      
      initializeNTP();
      printCurrentTime();
      
      server.on("/", handleConfigPage);
      server.on("/save", handleSaveConfig);
      server.on("/status", handleSTATUSJson);
      server.on("/update", HTTP_GET, handleOTAPage);
      server.on("/update", HTTP_POST, handleOTAUploadComplete, handleOTAFileUpload);
      server.on("/log", handleLogPage);
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
      
      if (attempt < 3) {
        delay(2000);
      }
    }
  }
  
  addLog("✗ All 3 connection attempts failed!");
  addLog("Starting config mode...");
  
  currentMode = MODE_AP_CONFIG;
  wifiConnectAttempts = 3;

  WiFi.disconnect();
  delay(200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266-Config", "12345678");
  
  setAllLEDs(255, 255, 0);
  
  server.on("/", handleConfigPage);
  server.on("/save", handleSaveConfig);
  server.on("/status", handleSTATUSJson);
  server.on("/log", handleLogPage);
  server.on("/logdata", handleLogData);
  server.begin();
  
  addLog("✓ Web server started at http://192.168.4.1");
  addLog("✓ SSID: OnAIR-Config | Password: 12345678");
}

// ============== WEB HANDLERS ==============

void handleConfigPage() {
  String html = "<!DOCTYPE html>";
  html += "<html><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>OnAIR Device WiFi Config</title>";
  html += "<style>";
  html += "body { font-family: Arial; margin: 40px; background: #f0f0f0; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; }";
  html += "h2 { color: #555; font-size: 16px; margin-top: 25px; padding-bottom: 10px; border-bottom: 2px solid #ddd; }";
  html += ".form-group { margin-bottom: 15px; }";
  html += "label { display: block; margin-bottom: 5px; color: #555; font-weight: bold; }";
  html += "input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type=\"checkbox\"] { width: auto; margin-right: 10px; }";
  html += ".checkbox-group { display: flex; flex-wrap: wrap; gap: 15px; margin-top: 10px; }";
  html += ".checkbox-item { display: flex; align-items: center; }";
  html += ".time-inputs { display: flex; gap: 10px; align-items: center; }";
  html += ".time-inputs input { max-width: 80px; }";
  html += "button { width: 100%; padding: 10px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 10px; }";
  html += "button:hover { background: #0056b3; }";
  html += ".status { margin-top: 20px; padding: 10px; background: #f9f9f9; border-left: 4px solid #007bff; border-radius: 4px; }";
  html += ".status-label { font-weight: bold; color: #333; }";
  html += ".error-box { margin-top: 20px; padding: 15px; background: #fee; border-left: 4px solid #dc3545; border-radius: 4px; }";
  html += ".error-title { font-weight: bold; color: #dc3545; margin-bottom: 5px; }";
  html += ".error-text { color: #721c24; font-size: 14px; }";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<h1>OnAIR Device Configuration</h1>";
  html += "<p style=\"text-align:center;color:#999;font-size:13px;margin-top:-10px;\">Firmware v" FIRMWARE_VERSION "</p>";
  
  if (wifiConnectAttempts > 0) {
    html += "<div class=\"error-box\">";
    html += "<div class=\"error-title\">Failed WiFi Attempts: " + String(wifiConnectAttempts) + "/3</div>";
    
    if (lastError != "") {
      html += "<div class=\"error-text\">WiFi Error: " + lastError + "</div>";
    } else {
      html += "<div class=\"error-text\">WiFi connection failed. Check SSID and password.</div>";
    }
    html += "</div>";
  }
  
  if (apiErrorCount > 0) {
    html += "<div class=\"error-box\">";
    html += "<div class=\"error-title\">Failed API Attempts: " + String(apiErrorCount) + "/5</div>";
    
    if (lastApiError != "") {
      html += "<div class=\"error-text\">API Error: " + lastApiError + "</div>";
    } else {
      html += "<div class=\"error-text\">API connection failed. Check the URL.</div>";
    }
    html += "</div>";
  }
  
  html += "<form onsubmit=\"saveConfig(event)\">";
  
  html += "<h2>WiFi Settings</h2>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"ssid\">WiFi SSID:</label>";
  html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" required placeholder=\"Network name\" value=\"" + String(config.ssid) + "\">";
  html += "</div>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"password\">WiFi Password:</label>";
  html += "<input type=\"password\" id=\"password\" name=\"password\" required placeholder=\"Password\" value=\"" + String(config.password) + "\">";
  html += "</div>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"apiUrl\">API URL:</label>";
  html += "<input type=\"text\" id=\"apiUrl\" name=\"apiUrl\" placeholder=\"http://localhost:3491/v1/status\" value=\"" + String(config.apiUrl) + "\">";
  html += "</div>";
  
  html += "<h2>LED Schedule</h2>";
  html += "<div class=\"form-group\">";
  html += "<label>";
  html += "<input type=\"checkbox\" id=\"scheduleEnabled\" name=\"scheduleEnabled\"" + String(schedule.enabled ? " checked" : "") + "> Enable Schedule";
  html += "</label>";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label>Working Hours:</label>";
  html += "<div class=\"time-inputs\">";
  html += "<input type=\"number\" id=\"startHour\" min=\"0\" max=\"23\" value=\"" + String(schedule.startHour) + "\" placeholder=\"Start hour\">";
  html += "<span>to</span>";
  html += "<input type=\"number\" id=\"endHour\" min=\"0\" max=\"23\" value=\"" + String(schedule.endHour) + "\" placeholder=\"End hour\">";
  html += "</div>";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label>Active Days:</label>";
  html += "<div class=\"checkbox-group\">";
  const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  for (int i = 0; i < 7; i++) {
    html += "<div class=\"checkbox-item\">";
    html += "<input type=\"checkbox\" id=\"day" + String(i) + "\" name=\"day" + String(i) + "\"" + String(schedule.days[i] ? " checked" : "") + ">";
    html += "<label for=\"day" + String(i) + "\" style=\"margin: 0;\">" + String(days[i]) + "</label>";
    html += "</div>";
  }
  html += "</div>";
  html += "</div>";
  
  html += "<button type=\"submit\">Save & Reconnect</button>";
  html += "</form>";
  html += "<div id=\"status\" class=\"status\" style=\"display:none;\"></div>";
  html += "<button onclick=\"location.href='/log'\" style=\"background:#0d6efd;margin-top:10px;\">&#128196; Event Log</button>";
  if (currentMode == MODE_STA_NORMAL) {
    html += "<button onclick=\"location.href='/update'\" style=\"background:#28a745;margin-top:10px;\">&#8593; Firmware Update (OTA)</button>";
  }
  html += "</div>";
  html += "<script>";
  html += "function saveConfig(event) {";
  html += "  event.preventDefault();";
  html += "  const ssid = document.getElementById(\"ssid\").value;";
  html += "  const password = document.getElementById(\"password\").value;";
  html += "  const apiUrl = document.getElementById(\"apiUrl\").value;";
  html += "  const scheduleEnabled = document.getElementById(\"scheduleEnabled\").checked;";
  html += "  const startHour = parseInt(document.getElementById(\"startHour\").value) || 0;";
  html += "  const endHour = parseInt(document.getElementById(\"endHour\").value) || 23;";
  html += "  const days = [];";
  html += "  for (let i = 0; i < 7; i++) {";
  html += "    days.push(document.getElementById(\"day\" + i).checked);";
  html += "  }";
  html += "  const payload = {";
  html += "    ssid: ssid,";
  html += "    password: password,";
  html += "    apiUrl: apiUrl,";
  html += "    schedule: {";
  html += "      enabled: scheduleEnabled,";
  html += "      startHour: startHour,";
  html += "      endHour: endHour,";
  html += "      days: days";
  html += "    }";
  html += "  };";
  html += "  fetch(\"/save\", {";
  html += "    method: \"POST\",";
  html += "    headers: {\"Content-Type\": \"application/json\"},";
  html += "    body: JSON.stringify(payload)";
  html += "  })";
  html += "  .then(r => r.text())";
  html += "  .then(msg => {";
  html += "    const statusDiv = document.getElementById(\"status\");";
  html += "    statusDiv.style.display = \"block\";";
  html += "    statusDiv.innerHTML = \"<span class=\\\"status-label\\\">Saved! Restarting...</span>\";";
  html += "    statusDiv.style.borderLeftColor = \"#28a745\";";
  html += "  })";
  html += "  .catch(err => {";
  html += "    const statusDiv = document.getElementById(\"status\");";
  html += "    statusDiv.style.display = \"block\";";
  html += "    statusDiv.innerHTML = \"<span class=\\\"status-label\\\">Error!</span>\";";
  html += "    statusDiv.style.borderLeftColor = \"#dc3545\";";
  html += "  });";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
      strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
      strlcpy(config.password, doc["password"] | "", sizeof(config.password));
      strlcpy(config.apiUrl, doc["apiUrl"] | "", sizeof(config.apiUrl));
      
      if (doc.containsKey("schedule")) {
        schedule.enabled = doc["schedule"]["enabled"] | true;
        schedule.startHour = doc["schedule"]["startHour"] | 0;
        schedule.endHour = doc["schedule"]["endHour"] | 23;
        
        if (doc["schedule"].containsKey("days")) {
          JsonArray daysArray = doc["schedule"]["days"];
          for (int i = 0; i < 7 && i < daysArray.size(); i++) {
            schedule.days[i] = daysArray[i] | true;
          }
        }
      }
      
      saveConfig();
      
      wifiConnectAttempts = 0;
      lastError = "";
      
      apiErrorCount = 0;
      lastApiError = "";
      apiEverSucceeded = false;
      isApiTransientError = false;
      
      server.send(200, "text/plain", "OK");
      
      delay(1000);
      ESP.restart();
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleSTATUSJson() {
  StaticJsonDocument<256> doc;
  doc["mode"] = (currentMode == MODE_AP_CONFIG) ? "AP" : "STA";
  doc["ssid"] = config.ssid;
  doc["apiUrl"] = config.apiUrl;
  
  if (currentMode == MODE_STA_NORMAL) {
    doc["wifiStatus"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    doc["ip"] = WiFi.localIP().toString();
    doc["callState"] = callState;
    doc["muteState"] = muteState;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ============== OTA HANDLERS ==============

void handleOTAPage() {
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>OTA Update</title>";
  html += "<style>";
  html += "body { font-family: Arial; margin: 40px; background: #f0f0f0; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; }";
  html += "input[type=file] { display: block; width: 100%; margin: 15px 0; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "button { width: 100%; padding: 10px; background: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 5px; }";
  html += "button:hover { background: #1e7e34; }";
  html += ".back { background: #6c757d; margin-top: 10px; }";
  html += ".back:hover { background: #545b62; }";
  html += ".info { margin: 15px 0; padding: 12px; background: #e8f4fd; border-left: 4px solid #007bff; border-radius: 4px; font-size: 13px; line-height: 1.5; }";
  html += "#progressBox { display: none; margin-top: 15px; }";
  html += "progress { width: 100%; height: 22px; border-radius: 4px; }";
  html += "#statusText { margin-top: 8px; font-weight: bold; color: #333; }";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<h1>Firmware Update</h1>";
  html += "<div class=\"info\">";
  html += "Upload a compiled <b>.bin</b> file to update the firmware.<br>";
  html += "During the update: LEDs show <b>white</b> progress bar.<br>";
  html += "On success: LEDs flash <b>green</b>, device reboots automatically.";
  html += "</div>";
  html += "<form id=\"uploadForm\">";
  html += "<input type=\"file\" id=\"fwFile\" accept=\".bin\" required>";
  html += "<button type=\"submit\">Upload Firmware</button>";
  html += "</form>";
  html += "<button class=\"back\" onclick=\"location.href='/'\">&#8592; Back to Settings</button>";
  html += "<div id=\"progressBox\">";
  html += "<progress id=\"bar\" value=\"0\" max=\"100\"></progress>";
  html += "<div id=\"statusText\">Uploading...</div>";
  html += "</div>";
  html += "<script>";
  html += "document.getElementById('uploadForm').onsubmit=function(e){";
  html += "  e.preventDefault();";
  html += "  var f=document.getElementById('fwFile').files[0];";
  html += "  if(!f)return;";
  html += "  var fd=new FormData();fd.append('firmware',f);";
  html += "  var xhr=new XMLHttpRequest();";
  html += "  xhr.open('POST','/update',true);";
  html += "  document.getElementById('progressBox').style.display='block';";
  html += "  xhr.upload.onprogress=function(e){";
  html += "    if(e.lengthComputable){var p=Math.round(e.loaded*100/e.total);";
  html += "    document.getElementById('bar').value=p;";
  html += "    document.getElementById('statusText').textContent='Uploading: '+p+'%';}";
  html += "  };";
  html += "  xhr.onload=function(){";
  html += "    if(xhr.status===200){";
  html += "      document.getElementById('bar').value=100;";
  html += "      document.getElementById('statusText').textContent='Done! Device is rebooting...';";
  html += "    }else{document.getElementById('statusText').textContent='Error: '+xhr.responseText;}";
  html += "  };";
  html += "  xhr.onerror=function(){document.getElementById('statusText').textContent='Upload failed!';};";
  html += "  xhr.send(fd);";
  html += "};";
  html += "</script>";
  html += "</div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleOTAFileUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    isOTAInProgress = true;
    lastLEDMode = LED_OFF;
    addLog("=== WEB OTA: uploading " + upload.filename + " ===");
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(255, 255, 255));
    }
    strip.show();
    size_t freeSpace = ESP.getFreeSketchSpace();
    addLog("OTA free space: " + String(freeSpace) + " bytes");
    if (!Update.begin(freeSpace)) {
      addLog("✗ Update.begin() failed: " + String(Update.getErrorString()));
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.hasError()) return;   // abort silently if begin() failed
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      addLog("✗ Update.write() error: " + String(Update.getErrorString()));
    } else {
      uint32_t sz = Update.size();
      int ledsOn = sz > 0 ? (int)((Update.progress() * NUM_LEDS) / sz) : 0;
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, i < ledsOn ? strip.Color(255, 255, 255) : 0);
      }
      strip.show();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.hasError()) {
      addLog("✗ OTA aborted: " + String(Update.getErrorString()));
      return;
    }
    if (Update.end(true)) {
      addLog("✓ WEB OTA complete: " + String(upload.totalSize) + " bytes");
    } else {
      addLog("✗ Update.end() failed: " + String(Update.getErrorString()));
    }
  }
}

void handleOTAUploadComplete() {
  if (Update.hasError()) {
    String err = Update.getErrorString();
    server.send(500, "text/plain", "Update FAILED: " + err);
    isOTAInProgress = false;
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
    delay(2000);
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
    lastLEDMode = LED_OFF;
  } else {
    server.send(200, "text/plain", "Update OK! Rebooting...");
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(0, 255, 0));
    }
    strip.show();
    delay(800);
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
    delay(300);
    ESP.restart();
  }
}

// ============== LOG PAGE HANDLERS ==============

void handleLogPage() {
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<title>Event Log</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: monospace; background: #0d1117; color: #c9d1d9; }";
  html += ".header { background: #161b22; padding: 14px 20px; display: flex; align-items: center; gap: 12px; border-bottom: 1px solid #30363d; }";
  html += "h1 { font-size: 17px; color: #58a6ff; font-family: Arial; flex: 1; }";
  html += ".btn { padding: 6px 14px; border: 1px solid #30363d; border-radius: 4px; cursor: pointer; font-size: 13px; background: #21262d; color: #c9d1d9; text-decoration: none; }";
  html += ".btn:hover { background: #30363d; }";
  html += ".dot { width: 9px; height: 9px; border-radius: 50%; background: #3fb950; }";
  html += ".dot.err { background: #ff7b72; }";
  html += ".log { padding: 10px 20px; }";
  html += ".entry { padding: 4px 0; border-bottom: 1px solid #161b22; font-size: 13px; line-height: 1.6; white-space: pre-wrap; word-break: break-all; }";
  html += ".ts { color: #484f58; margin-right: 8px; font-size: 11px; }";
  html += ".ok { color: #3fb950; }";
  html += ".er { color: #ff7b72; }";
  html += ".info { color: #e6edf3; }";
  html += ".empty { color: #484f58; padding: 40px; text-align: center; }";
  html += "</style></head><body>";
  html += "<div class=\"header\">";
  html += "<h1>&#128196; Event Log</h1>";
  html += "<a class=\"btn\" href=\"/\">&#8592; Settings</a>";
  html += "<div class=\"dot\" id=\"dot\"></div>";
  html += "</div>";
  html += "<div class=\"log\" id=\"log\"><div class=\"empty\">Loading...</div></div>";
  html += "<script>";
  html += "function fmt(ms){var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);return (h?h+'h ':'')+( m%60?(m%60)+'m ':'')+s%60+'s';}";
  html += "function cls(m){return m.indexOf('\\u2713')>=0?'ok':(m.indexOf('\\u2717')>=0?'er':'info');}";
  html += "function fetch_log(){fetch('/logdata').then(r=>r.json()).then(function(d){";
  html += "  document.getElementById('dot').className='dot';";
  html += "  var el=document.getElementById('log');";
  html += "  if(!d.entries||d.entries.length===0){el.innerHTML='<div class=\"empty\">No entries yet.</div>';return;}";
  html += "  var h=d.entries.slice().reverse().map(function(e){";
  html += "    var c=cls(e.msg);";
  html += "    return '<div class=\"entry\"><span class=\"ts\">['+fmt(e.ts)+']</span><span class=\"'+c+'\">'+e.msg+'</span></div>';";
  html += "  }).join('');";
  html += "  el.innerHTML=h;";
  html += "}).catch(function(){document.getElementById('dot').className='dot err';});}";
  html += "fetch_log();setInterval(fetch_log,3000);";
  html += "</script></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleLogData() {
  int total = (logCount < LOG_ENTRIES) ? logCount : LOG_ENTRIES;
  int start = (logCount < LOG_ENTRIES) ? 0 : logHead;

  String json = "{\"entries\":[";
  for (int i = 0; i < total; i++) {
    int idx = (start + i) % LOG_ENTRIES;
    if (i > 0) json += ",";
    json += "{\"ts\":";
    json += eventLog[idx].ts;
    json += ",\"msg\":\"";
    String msg = eventLog[idx].text;
    msg.replace("\\", "\\\\");
    msg.replace("\"", "\\\"");
    json += msg;
    json += "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ============== WiFi FUNCTIONS ==============

void connectToWiFi() {
  if (WiFi.isConnected()) {
    return;
  }
  
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

// ============== API FUNCTIONS ==============

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
    
    retryCount = 0;
    isInErrorState = false;
    isApiTransientError = false;
    
    if (!apiEverSucceeded) {
      addLog("✓ First successful API connection!");
    }
    
    apiEverSucceeded = true;
    apiErrorCount = 0;
    lastApiError = "";
    
  } else if (httpResponseCode == 404) {
    addLog("✗ Error 404: invalid URL!");
    lastApiError = "404 Not Found - check API URL";
    
      if (!apiEverSucceeded) {
      apiErrorCount++;
      addLog("API errors: " + String(apiErrorCount) + "/5");
      
      if (apiErrorCount >= 5) {
        addLog("✗ 5 failed API connection attempts!");
        if (!isInErrorState) {
          isInErrorState = true;
          lastErrorTime = millis();
          isApiTransientError = false;
          addLog("Pausing 5 minutes before retry...");
        }
      } else {
        isApiTransientError = true;
      }
    } else {
      if (!isInErrorState) {
        isInErrorState = true;
        lastErrorTime = millis();
        isApiTransientError = false;
        addLog("Pausing 5 minutes before retry...");
      }
    }
    
  } else {
    addLog("✗ HTTP error: " + String(httpResponseCode));
    Serial.print("✗ HTTP error: ");
    Serial.println(httpResponseCode);
    lastApiError = String("HTTP ") + String(httpResponseCode);
    
    if (apiEverSucceeded) {
      retryCount++;
      addLog("Retries: " + String(retryCount) + "/" + String(maxRetries));
      
      if (retryCount >= maxRetries) {
        addLog("Max retries reached, pausing 5 minutes...");
        isInErrorState = true;
        lastErrorTime = millis();
        retryCount = 0;
        isApiTransientError = false;
      } else {
        isApiTransientError = true;
      }
    } else {
      apiErrorCount++;
      addLog("API errors: " + String(apiErrorCount) + "/5");
      
      if (apiErrorCount >= 5) {
        addLog("✗ 5 failed API connection attempts!");
        if (!isInErrorState) {
          isInErrorState = true;
          lastErrorTime = millis();
          isApiTransientError = false;
          addLog("Pausing 5 minutes before retry...");
        }
      } else {
        isApiTransientError = true;
      }
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
    String newCall = doc["call"].as<String>();
    if (newCall != callState) {
      callState = newCall;
      addLog("Call: " + callState);
    } else {
      callState = newCall;
    }
  }
  
  if (doc.containsKey("mute")) {
    String newMute = doc["mute"].as<String>();
    if (newMute != muteState) {
      muteState = newMute;
      addLog("Mute: " + muteState);
    } else {
      muteState = newMute;
    }
  }
  
  Serial.println("LED state updated in memory");
}

// ============== LED FUNCTIONS ==============

void forceClearBuffer() {
  noInterrupts();
  for (int attempt = 0; attempt < 5; attempt++) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
  }
  interrupts();
  delay(5);
}

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
  noInterrupts();
  
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  
  strip.show();
  interrupts();
}

void setLEDColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < strip.numPixels()) {
    strip.setPixelColor(index, strip.Color(r, g, b));
  }
}

void clearAllLEDs() {
  currentLEDMode = LED_OFF;
  if (lastLEDMode != currentLEDMode) {
    forceClearBuffer();
    lastLEDMode = currentLEDMode;
  }
}

void ledBlink(LEDMode mode, uint8_t r, uint8_t g, uint8_t b) {
  currentLEDMode = mode;

  const unsigned long blinkInterval = 250;
  static unsigned long lastToggle = 0;
  static bool isOn = false;
  static bool initialized = false;

  if (lastLEDMode != currentLEDMode) {
    initialized = false;
    lastLEDMode = currentLEDMode;
  }

  if (!initialized) {
    forceClearBuffer();
    initialized = true;
    lastToggle = millis();
    isOn = false;
  }

  if (millis() - lastToggle >= blinkInterval) {
    lastToggle = millis();
    isOn = !isOn;

    noInterrupts();
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, isOn ? strip.Color(r, g, b) : 0);
    }
    strip.show();
    interrupts();
  }
}

void ledSolid(LEDMode mode, uint8_t r, uint8_t g, uint8_t b) {
  currentLEDMode = mode;

  const unsigned long refreshInterval = 1000;
  static unsigned long lastRefresh = 0;
  static bool initialized = false;

  if (lastLEDMode != currentLEDMode) {
    initialized = false;
    lastLEDMode = currentLEDMode;
  }

  if (!initialized || (millis() - lastRefresh >= refreshInterval)) {
    if (!initialized) forceClearBuffer();
    initialized = true;
    lastRefresh = millis();

    noInterrupts();
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    interrupts();
  }
}

void showRainbow() {
  currentLEDMode = LED_RAINBOW;
  
  const unsigned long rainbowSpeed = 80;
  static unsigned long lastToggle = 0;
  static int rainbowPos = 0;
  static bool initialized = false;
  
  // Reset on mode change
  if (lastLEDMode != currentLEDMode) {
    initialized = false;
    lastLEDMode = currentLEDMode;
  }
  
  // Initialize once on first call
  if (!initialized) {
    forceClearBuffer();
    initialized = true;
    lastToggle = millis();
    rainbowPos = 0;
  }
  
  const int rainbowColors[7][3] = {
    {255, 0, 0},
    {255, 127, 0},
    {255, 255, 0},
    {0, 255, 0},
    {0, 127, 255},
    {0, 0, 255},
    {127, 0, 255}
  };
  
  if (millis() - lastToggle >= rainbowSpeed) {
    lastToggle = millis();
    rainbowPos++;
    if (rainbowPos >= NUM_LEDS * 2) {
      rainbowPos = 0;
    }
  }
  
  noInterrupts();
  for (int i = 0; i < NUM_LEDS; i++) {
    int colorIndex = (i + rainbowPos) % 7;
    strip.setPixelColor(i, strip.Color(rainbowColors[colorIndex][0], 
                                       rainbowColors[colorIndex][1], 
                                       rainbowColors[colorIndex][2]));
  }
  strip.show();
  interrupts();
}

void updateLEDDisplay() {
  if (!isScheduleActive()) {
    clearAllLEDs();
    return;
  }
  
  if (isInErrorState) {
    showRainbow();
    return;
  }
  
  if (isApiTransientError) {
    ledBlink(LED_BLUE_BLINK, 0, 0, 255);
    return;
  }
  
  if (callState == "inactive") {
    clearAllLEDs();
  } else if (callState == "active") {
    if (muteState == "inactive") {
      ledBlink(LED_RED_BLINK, 255, 0, 0);   // on call, mic live  → fast red blink
    } else {
      ledSolid(LED_RED_SOLID, 255, 0, 0);   // on call, muted     → steady red
    }
  }
}

bool isScheduleActive() {
  if (!schedule.enabled) {
    return true;
  }
  
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  
  int dayOfWeek = timeinfo->tm_wday;
  int scheduleDay = (dayOfWeek == 0) ? 6 : (dayOfWeek - 1);
  
  if (!schedule.days[scheduleDay]) {
    return false;
  }
  
  int currentHour = timeinfo->tm_hour;
  
  if (schedule.startHour <= schedule.endHour) {
    return (currentHour >= schedule.startHour && currentHour < schedule.endHour);
  } 
  else {
    return (currentHour >= schedule.startHour || currentHour < schedule.endHour);
  }
}

// ============== NTP AND TIME ==============

void initializeNTP() {
  addLog("=== TIME SYNC ===");
  Serial.println("Syncing time with NTP server (0.ua.pool.ntp.org)...");
  
  configTime(2 * 3600, 1 * 3600, "0.ua.pool.ntp.org", "1.ua.pool.ntp.org", "2.ua.pool.ntp.org");
  
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  tzset();
  
  int attempts = 0;
  time_t now = time(nullptr);
  while (now < 24 * 3600 && attempts < 50) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }
  Serial.println();
  
  if (now > 24 * 3600) {
    addLog("✓ Time synced successfully!");
  } else {
    addLog("✗ Failed to sync time with NTP");
  }
}

void printCurrentTime() {
  time_t now = time(nullptr);
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
