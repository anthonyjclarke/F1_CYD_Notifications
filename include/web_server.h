#pragma once

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "types.h"
#include "config_manager.h"
#include "f1_data.h"
#include "time_utils.h"
#include "display_renderer.h"

static AsyncWebServer server(WEB_SERVER_PORT);
static AppConfig* _webConfigPtr = nullptr;

// HTML page stored in PROGMEM (avoids LittleFS dependency for core UI)
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>F1 Display Config</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;padding:20px;max-width:600px;margin:0 auto}
h1{color:#e10600;text-align:center;margin-bottom:8px;font-size:1.6em}
h2{color:#e10600;margin:20px 0 10px;font-size:1.1em;border-bottom:1px solid #333;padding-bottom:5px}
.status{text-align:center;color:#888;margin-bottom:20px;font-size:0.85em}
label{display:block;margin:10px 0 4px;color:#ccc;font-size:0.9em}
input,select{width:100%;padding:10px;border:1px solid #333;border-radius:6px;background:#16213e;color:#eee;font-size:0.95em}
input:focus{border-color:#e10600;outline:none}
.range-wrap{display:flex;align-items:center;gap:10px}
.range-wrap input[type=range]{flex:1}
.range-val{min-width:36px;text-align:center;color:#e10600;font-weight:bold}
button{width:100%;padding:12px;margin-top:20px;border:none;border-radius:6px;background:#e10600;color:#fff;font-size:1em;font-weight:bold;cursor:pointer}
button:hover{background:#ff1801}
.msg{text-align:center;padding:10px;margin-top:10px;border-radius:6px;display:none}
.msg.ok{display:block;background:#0a3d0a;color:#4caf50}
.msg.err{display:block;background:#3d0a0a;color:#f44}
.info{background:#16213e;padding:12px;border-radius:6px;margin-top:15px;font-size:0.85em;color:#888}
.info span{color:#eee}
a{color:#e10600}
</style>
</head>
<body>
<h1>&#127987;&#65039; F1 Display</h1>
<div class="status" id="status">Loading...</div>

<form id="configForm">
<h2>Timezone &amp; NTP</h2>
<label for="tz">Timezone (IANA format)</label>
<input type="text" id="tz" name="tz" placeholder="Europe/London">
<label for="ntp">NTP Server</label>
<input type="text" id="ntp" name="ntp" placeholder="pool.ntp.org">

<h2>Display</h2>
<label>Brightness (0 = Auto)</label>
<div class="range-wrap">
<input type="range" id="bright" name="bright" min="0" max="255" value="128">
<span class="range-val" id="brightVal">128</span>
</div>

<h2>Telegram Notifications</h2>
<label for="bot">Bot Token</label>
<input type="text" id="bot" name="bot" placeholder="123456:ABC-DEF...">
<label for="chat">Chat ID</label>
<input type="text" id="chat" name="chat" placeholder="123456789">

<button type="submit">Save Configuration</button>
</form>
<div class="msg" id="msg"></div>

<div class="info">
<strong>Links:</strong><br>
<a href="/update">Firmware Update (OTA)</a><br><br>
<strong>Status:</strong><br>
Next Race: <span id="nextRace">-</span><br>
Free Heap: <span id="heap">-</span><br>
Uptime: <span id="uptime">-</span>
</div>

<script>
const $ = id => document.getElementById(id);
const bright = $('bright');
bright.oninput = () => $('brightVal').textContent = bright.value;

async function loadConfig() {
  try {
    const r = await fetch('/api/config');
    const c = await r.json();
    $('tz').value = c.tz || '';
    $('ntp').value = c.ntp || '';
    $('bot').value = c.bot || '';
    $('chat').value = c.chat || '';
    bright.value = c.bright || 128;
    $('brightVal').textContent = bright.value;
  } catch(e) { console.error(e); }
}

async function loadStatus() {
  try {
    const r = await fetch('/api/status');
    const s = await r.json();
    $('status').textContent = 'Connected - ' + s.ip;
    $('nextRace').textContent = s.nextRace || '-';
    $('heap').textContent = s.heap || '-';
    $('uptime').textContent = s.uptime || '-';
  } catch(e) { $('status').textContent = 'Connection error'; }
}

$('configForm').onsubmit = async (e) => {
  e.preventDefault();
  const msg = $('msg');
  try {
    const body = new URLSearchParams({
      tz: $('tz').value,
      ntp: $('ntp').value,
      bot: $('bot').value,
      chat: $('chat').value,
      bright: bright.value
    });
    const r = await fetch('/api/config', {method:'POST', body});
    if (r.ok) {
      msg.className = 'msg ok';
      msg.textContent = 'Configuration saved!';
    } else {
      msg.className = 'msg err';
      msg.textContent = 'Save failed';
    }
  } catch(e) {
    msg.className = 'msg err';
    msg.textContent = 'Error: ' + e.message;
  }
  setTimeout(() => msg.style.display = 'none', 3000);
};

loadConfig();
loadStatus();
setInterval(loadStatus, 10000);
</script>
</body>
</html>
)rawliteral";

void setupWebServer(AppConfig& cfg) {
    _webConfigPtr = &cfg;

    // Serve config page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // GET config as JSON
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!_webConfigPtr) { request->send(500); return; }
        JsonDocument doc;
        doc["tz"]     = _webConfigPtr->timezone;
        doc["ntp"]    = _webConfigPtr->ntpServer;
        doc["bot"]    = _webConfigPtr->botToken;
        doc["chat"]   = _webConfigPtr->chatId;
        doc["bright"] = _webConfigPtr->brightness;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST config update
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!_webConfigPtr) { request->send(500); return; }

        if (request->hasParam("tz", true))
            strlcpy(_webConfigPtr->timezone, request->getParam("tz", true)->value().c_str(),
                    sizeof(_webConfigPtr->timezone));
        if (request->hasParam("ntp", true))
            strlcpy(_webConfigPtr->ntpServer, request->getParam("ntp", true)->value().c_str(),
                    sizeof(_webConfigPtr->ntpServer));
        if (request->hasParam("bot", true))
            strlcpy(_webConfigPtr->botToken, request->getParam("bot", true)->value().c_str(),
                    sizeof(_webConfigPtr->botToken));
        if (request->hasParam("chat", true))
            strlcpy(_webConfigPtr->chatId, request->getParam("chat", true)->value().c_str(),
                    sizeof(_webConfigPtr->chatId));
        if (request->hasParam("bright", true))
            _webConfigPtr->brightness = request->getParam("bright", true)->value().toInt();

        _webConfigPtr->telegramEnabled = strlen(_webConfigPtr->botToken) > 0;

        // Apply brightness immediately
        updateBrightness(_webConfigPtr->brightness);

        // Re-init timezone if changed
        initTime(_webConfigPtr->timezone, _webConfigPtr->ntpServer);

        saveConfig(*_webConfigPtr);
        request->send(200, "application/json", "{\"ok\":true}");
        Serial.println(F("[Web] Config updated"));
    });

    // GET status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        RaceData& race = getCurrentRace();
        doc["nextRace"] = String(race.name) + " (R" + String(race.round) + ")";
        doc["heap"]     = String(ESP.getFreeHeap() / 1024) + " KB";

        unsigned long secs = millis() / 1000;
        char uptime[32];
        snprintf(uptime, sizeof(uptime), "%lud %luh %lum",
                 secs / 86400, (secs % 86400) / 3600, (secs % 3600) / 60);
        doc["uptime"] = uptime;
        doc["ip"]     = WiFi.localIP().toString();

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // ElegantOTA
    ElegantOTA.begin(&server);

    server.begin();
    Serial.printf("[Web] Server started on port %d\n", WEB_SERVER_PORT);
}
