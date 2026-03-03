#include "web_handlers.h"
#include "config.h"
#include <Updater.h>

void handleConfigPage() {
  String html = "<!DOCTYPE html>";
  html.reserve(3200);
  html += F("<html><head><meta charset=\"UTF-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += F("<title>OnAIR Device WiFi Config</title>");
  html += F("<style>");
  html += F("body { font-family: Arial; margin: 40px; background: #f0f0f0; }");
  html += F(".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
  html += F("h1 { color: #333; text-align: center; }");
  html += F("h2 { color: #555; font-size: 16px; margin-top: 25px; padding-bottom: 10px; border-bottom: 2px solid #ddd; }");
  html += F(".form-group { margin-bottom: 15px; }");
  html += F("label { display: block; margin-bottom: 5px; color: #555; font-weight: bold; }");
  html += F("input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }");
  html += F("input[type=\"checkbox\"] { width: auto; margin-right: 10px; }");
  html += F(".checkbox-group { display: flex; flex-wrap: wrap; gap: 15px; margin-top: 10px; }");
  html += F(".checkbox-item { display: flex; align-items: center; }");
  html += F(".time-inputs { display: flex; gap: 10px; align-items: center; }");
  html += F(".time-inputs input { max-width: 80px; }");
  html += F("button { width: 100%; padding: 10px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 10px; }");
  html += F("button:hover { background: #0056b3; }");
  html += F(".status { margin-top: 20px; padding: 10px; background: #f9f9f9; border-left: 4px solid #007bff; border-radius: 4px; }");
  html += F(".status-label { font-weight: bold; color: #333; }");
  html += F(".error-box { margin-top: 20px; padding: 15px; background: #fee; border-left: 4px solid #dc3545; border-radius: 4px; }");
  html += F(".error-title { font-weight: bold; color: #dc3545; margin-bottom: 5px; }");
  html += F(".error-text { color: #721c24; font-size: 14px; }");
  html += F("</style></head><body>");
  html += F("<div class=\"container\">");
  html += F("<h1>OnAIR Device Configuration</h1>");
  html += "<p style=\"text-align:center;color:#999;font-size:13px;margin-top:-10px;\">Firmware v" FIRMWARE_VERSION "</p>";

  if (wifiConnectAttempts > 0) {
    html += F("<div class=\"error-box\">");
    html += "<div class=\"error-title\">Failed WiFi Attempts: " + String(wifiConnectAttempts) + "/3</div>";
    if (lastError != "") {
      html += "<div class=\"error-text\">WiFi Error: " + lastError + "</div>";
    } else {
      html += F("<div class=\"error-text\">WiFi connection failed. Check SSID and password.</div>");
    }
    html += F("</div>");
  }

  if (apiErrorCount > 0) {
    html += F("<div class=\"error-box\">");
    html += "<div class=\"error-title\">Failed API Attempts: " + String(apiErrorCount) + "/5</div>";
    if (lastApiError != "") {
      html += "<div class=\"error-text\">API Error: " + lastApiError + "</div>";
    } else {
      html += F("<div class=\"error-text\">API connection failed. Check the URL.</div>");
    }
    html += F("</div>");
  }

  html += F("<form onsubmit=\"saveConfig(event)\">");
  html += F("<h2>WiFi Settings</h2>");
  html += F("<div class=\"form-group\">");
  html += F("<label for=\"ssid\">WiFi SSID:</label>");
  html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" required placeholder=\"Network name\" value=\"" + String(config.ssid) + "\">";
  html += F("</div>");
  html += F("<div class=\"form-group\">");
  html += F("<label for=\"password\">WiFi Password:</label>");
  html += "<input type=\"password\" id=\"password\" name=\"password\" required placeholder=\"Password\" value=\"" + String(config.password) + "\">";
  html += F("</div>");
  html += F("<div class=\"form-group\">");
  html += F("<label for=\"apiUrl\">API URL:</label>");
  html += "<input type=\"text\" id=\"apiUrl\" name=\"apiUrl\" placeholder=\"http://localhost:3491/v1/status\" value=\"" + String(config.apiUrl) + "\">";
  html += F("</div>");

  html += F("<h2>LED Schedule</h2>");
  html += F("<div class=\"form-group\">");
  html += F("<label>");
  html += "<input type=\"checkbox\" id=\"scheduleEnabled\" name=\"scheduleEnabled\"" + String(schedule.enabled ? " checked" : "") + "> Enable Schedule";
  html += F("</label>");
  html += F("</div>");

  html += F("<div class=\"form-group\">");
  html += F("<label>Working Hours:</label>");
  html += F("<div class=\"time-inputs\">");
  html += "<input type=\"number\" id=\"startHour\" min=\"0\" max=\"23\" value=\"" + String(schedule.startHour) + "\" placeholder=\"Start hour\">";
  html += F("<span>to</span>");
  html += "<input type=\"number\" id=\"endHour\" min=\"0\" max=\"23\" value=\"" + String(schedule.endHour) + "\" placeholder=\"End hour\">";
  html += F("</div>");
  html += F("</div>");

  html += F("<div class=\"form-group\">");
  html += F("<label>Active Days:</label>");
  html += F("<div class=\"checkbox-group\">");
  const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  for (int i = 0; i < 7; i++) {
    html += F("<div class=\"checkbox-item\">");
    html += "<input type=\"checkbox\" id=\"day" + String(i) + "\" name=\"day" + String(i) + "\"" + String(schedule.days[i] ? " checked" : "") + ">";
    html += "<label for=\"day" + String(i) + "\" style=\"margin: 0;\">" + String(days[i]) + "</label>";
    html += F("</div>");
  }
  html += F("</div>");
  html += F("</div>");

  html += F("<button type=\"submit\">Save & Reconnect</button>");
  html += F("</form>");
  html += F("<div id=\"status\" class=\"status\" style=\"display:none;\"></div>");
  html += F("<button onclick=\"location.href='/log'\" style=\"background:#0d6efd;margin-top:10px;\">&#128196; Event Log</button>");
  if (currentMode == MODE_STA_NORMAL) {
    html += F("<button onclick=\"location.href='/update'\" style=\"background:#28a745;margin-top:10px;\">&#8593; Firmware Update (OTA)</button>");
  }
  html += F("</div>");
  html += F("<script>");
  html += F("function saveConfig(event) {");
  html += F("  event.preventDefault();");
  html += F("  const ssid = document.getElementById(\"ssid\").value;");
  html += F("  const password = document.getElementById(\"password\").value;");
  html += F("  const apiUrl = document.getElementById(\"apiUrl\").value;");
  html += F("  const scheduleEnabled = document.getElementById(\"scheduleEnabled\").checked;");
  html += F("  const startHour = parseInt(document.getElementById(\"startHour\").value) || 0;");
  html += F("  const endHour = parseInt(document.getElementById(\"endHour\").value) || 23;");
  html += F("  const days = [];");
  html += F("  for (let i = 0; i < 7; i++) {");
  html += F("    days.push(document.getElementById(\"day\" + i).checked);");
  html += F("  }");
  html += F("  const payload = {");
  html += F("    ssid: ssid,");
  html += F("    password: password,");
  html += F("    apiUrl: apiUrl,");
  html += F("    schedule: {");
  html += F("      enabled: scheduleEnabled,");
  html += F("      startHour: startHour,");
  html += F("      endHour: endHour,");
  html += F("      days: days");
  html += F("    }");
  html += F("  };");
  html += F("  fetch(\"/save\", {");
  html += F("    method: \"POST\",");
  html += F("    headers: {\"Content-Type\": \"application/json\"},");
  html += F("    body: JSON.stringify(payload)");
  html += F("  })");
  html += F("  .then(r => r.text())");
  html += F("  .then(msg => {");
  html += F("    const statusDiv = document.getElementById(\"status\");");
  html += F("    statusDiv.style.display = \"block\";");
  html += F("    statusDiv.innerHTML = \"<span class=\\\"status-label\\\">Saved! Restarting...</span>\";");
  html += F("    statusDiv.style.borderLeftColor = \"#28a745\";");
  html += F("  })");
  html += F("  .catch(err => {");
  html += F("    const statusDiv = document.getElementById(\"status\");");
  html += F("    statusDiv.style.display = \"block\";");
  html += F("    statusDiv.innerHTML = \"<span class=\\\"status-label\\\">Error!</span>\";");
  html += F("    statusDiv.style.borderLeftColor = \"#dc3545\";");
  html += F("  });");
  html += F("}");
  html += F("</script>");
  html += F("</body></html>");

  server.send(200, "text/html; charset=utf-8", html);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
      applyJsonToConfig(doc);
      saveConfig();

      wifiConnectAttempts = 0;
      lastError           = "";
      apiErrorCount       = 0;
      lastApiError        = "";
      apiEverSucceeded    = false;
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
  doc["mode"]   = (currentMode == MODE_AP_CONFIG) ? "AP" : "STA";
  doc["ssid"]   = config.ssid;
  doc["apiUrl"] = config.apiUrl;

  if (currentMode == MODE_STA_NORMAL) {
    doc["wifiStatus"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    doc["ip"]         = WiFi.localIP().toString();
    doc["callState"]  = (callState == CALL_ACTIVE) ? "active"   : "inactive";
    doc["muteState"]  = (muteState == MIC_MUTED)   ? "active"   : "inactive";
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ============== OTA HANDLERS ==============

void handleOTAPage() {
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
  html.reserve(1800);
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += F("<title>OTA Update</title>");
  html += F("<style>");
  html += F("body { font-family: Arial; margin: 40px; background: #f0f0f0; }");
  html += F(".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
  html += F("h1 { color: #333; text-align: center; }");
  html += F("input[type=file] { display: block; width: 100%; margin: 15px 0; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }");
  html += F("button { width: 100%; padding: 10px; background: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 5px; }");
  html += F("button:hover { background: #1e7e34; }");
  html += F(".back { background: #6c757d; margin-top: 10px; }");
  html += F(".back:hover { background: #545b62; }");
  html += F(".info { margin: 15px 0; padding: 12px; background: #e8f4fd; border-left: 4px solid #007bff; border-radius: 4px; font-size: 13px; line-height: 1.5; }");
  html += F("#progressBox { display: none; margin-top: 15px; }");
  html += F("progress { width: 100%; height: 22px; border-radius: 4px; }");
  html += F("#statusText { margin-top: 8px; font-weight: bold; color: #333; }");
  html += F("</style></head><body>");
  html += F("<div class=\"container\">");
  html += F("<h1>Firmware Update</h1>");
  html += F("<div class=\"info\">");
  html += F("Upload a compiled <b>.bin</b> file to update the firmware.<br>");
  html += F("During the update: LEDs show <b>white</b> progress bar.<br>");
  html += F("On success: LEDs flash <b>green</b>, device reboots automatically.");
  html += F("</div>");
  html += F("<form id=\"uploadForm\">");
  html += F("<input type=\"file\" id=\"fwFile\" accept=\".bin\" required>");
  html += F("<button type=\"submit\">Upload Firmware</button>");
  html += F("</form>");
  html += F("<button class=\"back\" onclick=\"location.href='/'\">&#8592; Back to Settings</button>");
  html += F("<div id=\"progressBox\">");
  html += F("<progress id=\"bar\" value=\"0\" max=\"100\"></progress>");
  html += F("<div id=\"statusText\">Uploading...</div>");
  html += F("</div>");
  html += F("<script>");
  html += F("document.getElementById('uploadForm').onsubmit=function(e){");
  html += F("  e.preventDefault();");
  html += F("  var f=document.getElementById('fwFile').files[0];");
  html += F("  if(!f)return;");
  html += F("  var fd=new FormData();fd.append('firmware',f);");
  html += F("  var xhr=new XMLHttpRequest();");
  html += F("  xhr.open('POST','/update',true);");
  html += F("  document.getElementById('progressBox').style.display='block';");
  html += F("  xhr.upload.onprogress=function(e){");
  html += F("    if(e.lengthComputable){var p=Math.round(e.loaded*100/e.total);");
  html += F("    document.getElementById('bar').value=p;");
  html += F("    document.getElementById('statusText').textContent='Uploading: '+p+'%';}");
  html += F("  };");
  html += F("  xhr.onload=function(){");
  html += F("    if(xhr.status===200){");
  html += F("      document.getElementById('bar').value=100;");
  html += F("      document.getElementById('statusText').textContent='Done! Device is rebooting...';");
  html += F("    }else{document.getElementById('statusText').textContent='Error: '+xhr.responseText;}");
  html += F("  };");
  html += F("  xhr.onerror=function(){document.getElementById('statusText').textContent='Upload failed!';};");
  html += F("  xhr.send(fd);");
  html += F("};");
  html += F("</script>");
  html += F("</div></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleOTAFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    isOTAInProgress = true;
    lastLEDMode     = LED_OFF;
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
    if (Update.hasError()) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      addLog("✗ Update.write() error: " + String(Update.getErrorString()));
    } else {
      uint32_t sz    = Update.size();
      int      ledsOn = sz > 0 ? (int)((Update.progress() * NUM_LEDS) / sz) : 0;
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
  html.reserve(1200);
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>Event Log</title>");
  html += F("<style>");
  html += F("* { box-sizing: border-box; margin: 0; padding: 0; }");
  html += F("body { font-family: monospace; background: #0d1117; color: #c9d1d9; }");
  html += F(".header { background: #161b22; padding: 14px 20px; display: flex; align-items: center; gap: 12px; border-bottom: 1px solid #30363d; }");
  html += F("h1 { font-size: 17px; color: #58a6ff; font-family: Arial; flex: 1; }");
  html += F(".btn { padding: 6px 14px; border: 1px solid #30363d; border-radius: 4px; cursor: pointer; font-size: 13px; background: #21262d; color: #c9d1d9; text-decoration: none; }");
  html += F(".btn:hover { background: #30363d; }");
  html += F(".dot { width: 9px; height: 9px; border-radius: 50%; background: #3fb950; }");
  html += F(".dot.err { background: #ff7b72; }");
  html += F(".log { padding: 10px 20px; }");
  html += F(".entry { padding: 4px 0; border-bottom: 1px solid #161b22; font-size: 13px; line-height: 1.6; white-space: pre-wrap; word-break: break-all; }");
  html += F(".ts { color: #484f58; margin-right: 8px; font-size: 11px; }");
  html += F(".ok { color: #3fb950; }");
  html += F(".er { color: #ff7b72; }");
  html += F(".info { color: #e6edf3; }");
  html += F(".empty { color: #484f58; padding: 40px; text-align: center; }");
  html += F("</style></head><body>");
  html += F("<div class=\"header\">");
  html += F("<h1>&#128196; Event Log</h1>");
  html += F("<a class=\"btn\" href=\"/\">&#8592; Settings</a>");
  html += F("<div class=\"dot\" id=\"dot\"></div>");
  html += F("</div>");
  html += F("<div class=\"log\" id=\"log\"><div class=\"empty\">Loading...</div></div>");
  html += F("<script>");
  html += F("function fmt(ms){var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);return (h?h+'h ':'')+( m%60?(m%60)+'m ':'')+s%60+'s';}");
  html += F("function cls(m){return m.indexOf('\\u2713')>=0?'ok':(m.indexOf('\\u2717')>=0?'er':'info');}");
  html += F("function fetch_log(){fetch('/logdata').then(r=>r.json()).then(function(d){");
  html += F("  document.getElementById('dot').className='dot';");
  html += F("  var el=document.getElementById('log');");
  html += F("  if(!d.entries||d.entries.length===0){el.innerHTML='<div class=\"empty\">No entries yet.</div>';return;}");
  html += F("  var h=d.entries.slice().reverse().map(function(e){");
  html += F("    var c=cls(e.msg);");
  html += F("    return '<div class=\"entry\"><span class=\"ts\">['+fmt(e.ts)+']</span><span class=\"'+c+'\">'+e.msg+'</span></div>';");
  html += F("  }).join('');");
  html += F("  el.innerHTML=h;");
  html += F("}).catch(function(){document.getElementById('dot').className='dot err';});}");
  html += F("fetch_log();setInterval(fetch_log,3000);");
  html += F("</script></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleLogData() {
  int total = (logCount < LOG_ENTRIES) ? logCount : LOG_ENTRIES;
  int start = (logCount < LOG_ENTRIES) ? 0 : logHead;

  String json = "{\"entries\":[";
  for (int i = 0; i < total; i++) {
    int    idx = (start + i) % LOG_ENTRIES;
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
