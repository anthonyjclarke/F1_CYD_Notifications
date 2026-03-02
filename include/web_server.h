#pragma once

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "types.h"
#include "debug.h"
#include "config_manager.h"
#include "f1_data.h"
#include "f1_logo.h"
#include "time_utils.h"
#include "display_renderer.h"
#include "screenshot_capture.h"

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
#logoWrap{text-align:center;margin-bottom:10px}
canvas{width:236px;height:128px;border-radius:8px;background:#fff;box-shadow:0 2px 12px rgba(0,0,0,0.4)}
h2{color:#e10600;margin:20px 0 10px;font-size:1.1em;border-bottom:1px solid #333;padding-bottom:5px}
.status{text-align:center;color:#888;margin-bottom:14px;font-size:0.85em}
label{display:block;margin:10px 0 4px;color:#ccc;font-size:0.9em}
input,select{width:100%;padding:10px;border:1px solid #333;border-radius:6px;background:#16213e;color:#eee;font-size:0.95em}
input:focus{border-color:#e10600;outline:none}
.range-wrap{display:flex;align-items:center;gap:10px}
.range-wrap input[type=range]{flex:1}
.range-val{min-width:36px;text-align:center;color:#e10600;font-weight:bold}
button{width:100%;padding:12px;margin-top:20px;border:none;border-radius:6px;background:#e10600;color:#fff;font-size:1em;font-weight:bold;cursor:pointer}
button:hover{background:#ff1801}
.btn-sm{width:auto;padding:8px 16px;margin-top:0}
.msg{text-align:center;padding:10px;margin-top:10px;border-radius:6px;display:none}
.msg.ok{display:block;background:#0a3d0a;color:#4caf50}
.msg.err{display:block;background:#3d0a0a;color:#f44}
.info{background:#16213e;padding:12px;border-radius:6px;margin-top:15px;font-size:0.85em;color:#888}
.info span{color:#eee}
.dbg-row{display:flex;align-items:center;gap:8px;margin-top:8px}
.dbg-row select{width:auto;flex:1;padding:8px}
a{color:#e10600}
.tabs{display:flex;gap:6px;margin-bottom:16px}
.tab-btn{flex:1;padding:10px;border:1px solid #333;border-radius:6px;background:#16213e;color:#888;font-size:0.95em;cursor:pointer;font-weight:bold}
.tab-btn.active{background:#e10600;color:#fff;border-color:#e10600}
.sch-hdr{background:#16213e;padding:12px;border-radius:6px;margin-bottom:12px}
.sch-hdr .rname{color:#eee;font-weight:bold;font-size:1em}
.sch-hdr .rloc{color:#888;font-size:0.85em;margin-top:3px}
.sprint-badge{display:inline-block;background:#ffd700;color:#000;font-size:0.7em;padding:2px 6px;border-radius:4px;margin-left:8px;font-weight:bold;vertical-align:middle}
table{width:100%;border-collapse:collapse;font-size:0.85em}
th{color:#e10600;text-align:left;padding:8px 4px;border-bottom:1px solid #333}
td{padding:7px 4px;border-bottom:1px solid #1e1e2e}
.s-next td{color:#4caf50}
.s-gp td:first-child{color:#e10600;font-weight:bold}
.s-sprint td:first-child{color:#ffd700;font-weight:bold}
.s-past td{color:#3a3a4a}
td.cd{font-variant-numeric:tabular-nums;color:#666;font-size:0.8em;white-space:nowrap}
.s-next td.cd{color:#4caf50;font-weight:bold}
/* Season calendar */
#seasonTable td{padding:6px 4px;font-size:0.85em}
.r-cur td{color:#ffd700;font-weight:bold}
.r-done td{color:#3a3a4a}
</style>
</head>
<body>

<div id="logoWrap"><canvas id="logo"></canvas></div>
<div class="status" id="status">Loading...</div>

<div class="tabs">
  <button class="tab-btn active" id="btn-cfg" onclick="showTab('cfg')">&#9881; Config</button>
  <button class="tab-btn" id="btn-sch" onclick="showTab('sch')">&#128197; Schedule</button>
</div>

<!-- Config Tab -->
<div id="tab-cfg">
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
<strong>Debug Level (serial output):</strong>
<div class="dbg-row">
<select id="dbg">
<option value="0">0 - Off</option>
<option value="1">1 - Error</option>
<option value="2">2 - Warn</option>
<option value="3">3 - Info (default)</option>
<option value="4">4 - Verbose</option>
</select>
<button type="button" class="btn-sm" onclick="setDbg()">Apply</button>
</div>
</div>

<div class="info">
<strong>Links:</strong><br>
<a href="/update">Firmware Update (OTA)</a><br><br>
<button type="button" class="btn-sm" onclick="captureShot()">Capture TFT Screenshot</button><br><br>
<strong>Status:</strong><br>
Free Heap: <span id="heap">-</span><br>
Uptime: <span id="uptime">-</span>
</div>
</div>

<!-- Schedule Tab -->
<div id="tab-sch" style="display:none">
<div class="sch-hdr">
<div class="rname" id="schName">Loading...</div>
<div class="rloc" id="schLoc"></div>
</div>
<table>
<thead><tr><th>Session</th><th>Day</th><th>Time</th><th>In</th></tr></thead>
<tbody id="schBody"></tbody>
</table>
<h2>2026 Season</h2>
<table id="seasonTable">
<thead><tr><th>Rd</th><th>Grand Prix</th><th>Location</th><th>Date</th></tr></thead>
<tbody id="seasonBody"><tr><td colspan="4" style="color:#555">Loading...</td></tr></tbody>
</table>
</div>

<script>
const $ = id => document.getElementById(id);
const bright = $('bright');
bright.oninput = () => $('brightVal').textContent = bright.value;

function showTab(t) {
  $('tab-cfg').style.display = t==='cfg' ? '' : 'none';
  $('tab-sch').style.display = t==='sch' ? '' : 'none';
  $('btn-cfg').className = 'tab-btn' + (t==='cfg' ? ' active' : '');
  $('btn-sch').className = 'tab-btn' + (t==='sch' ? ' active' : '');
}

async function renderLogo() {
  try {
    const r = await fetch('/logo.raw');
    const buf = await r.arrayBuffer();
    const px = new Uint16Array(buf);
    const c = $('logo');
    c.width = 118; c.height = 64;
    const ctx = c.getContext('2d');
    const img = ctx.createImageData(118, 64);
    for (let i = 0; i < px.length; i++) {
      const p = px[i];
      img.data[i*4]   = ((p >> 11) & 0x1F) << 3;
      img.data[i*4+1] = ((p >>  5) & 0x3F) << 2;
      img.data[i*4+2] =  (p        & 0x1F) << 3;
      img.data[i*4+3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  } catch(e) { $('logoWrap').style.display = 'none'; }
}

async function loadConfig() {
  try {
    const r = await fetch('/api/config');
    const c = await r.json();
    $('tz').value   = c.tz     || '';
    $('ntp').value  = c.ntp    || '';
    $('bot').value  = c.bot    || '';
    $('chat').value = c.chat   || '';
    bright.value    = c.bright || 128;
    $('brightVal').textContent = bright.value;
  } catch(e) { console.error(e); }
}

async function loadStatus() {
  try {
    const r = await fetch('/api/status');
    const s = await r.json();
    $('status').textContent = 'Connected \u2014 ' + s.ip;
    $('heap').textContent   = s.heap   || '-';
    $('uptime').textContent = s.uptime || '-';
  } catch(e) { $('status').textContent = 'Connection error'; }
}

async function loadDebug() {
  try {
    const r = await fetch('/api/debug');
    const d = await r.json();
    $('dbg').value = d.level;
  } catch(e) {}
}

async function setDbg() {
  try {
    await fetch('/api/debug', {method:'POST', body: new URLSearchParams({level: $('dbg').value})});
  } catch(e) {}
}

async function captureShot() {
  const msg = $('msg');
  try {
    const r = await fetch('/api/screenshot', {method:'POST'});
    const d = await r.json();
    msg.className = r.ok ? 'msg ok' : 'msg err';
    msg.textContent = r.ok
      ? ('Screenshot queued: ' + (d.path || '(writing...)'))
      : (d.error || 'Screenshot failed');
    msg.style.display = 'block';
  } catch(e) {
    msg.className = 'msg err';
    msg.textContent = 'Screenshot error: ' + e.message;
    msg.style.display = 'block';
  }
  setTimeout(() => msg.style.display = 'none', 3500);
}

$('configForm').onsubmit = async (e) => {
  e.preventDefault();
  const msg = $('msg');
  try {
    const body = new URLSearchParams({
      tz: $('tz').value, ntp: $('ntp').value,
      bot: $('bot').value, chat: $('chat').value,
      bright: bright.value
    });
    const r = await fetch('/api/config', {method:'POST', body});
    msg.className   = r.ok ? 'msg ok'  : 'msg err';
    msg.textContent = r.ok ? 'Configuration saved!' : 'Save failed';
  } catch(e) {
    msg.className = 'msg err'; msg.textContent = 'Error: ' + e.message;
  }
  setTimeout(() => msg.style.display = 'none', 3000);
};

// --- Schedule Tab ---
let schedSessions = [];

function fmtIn(utcSec) {
  const diff = utcSec - Math.floor(Date.now() / 1000);
  if (diff <= 0) return null;
  const d = Math.floor(diff / 86400);
  const h = Math.floor((diff % 86400) / 3600);
  const m = Math.floor((diff % 3600) / 60);
  const s = diff % 60;
  if (d > 0) return d + 'd ' + h + 'h';
  if (h > 0) return h + 'h ' + m + 'm';
  return m + 'm ' + String(s).padStart(2, '0') + 's';
}

async function loadSchedule() {
  try {
    const r = await fetch('/api/schedule');
    const d = await r.json();
    $('schName').innerHTML = 'Round ' + d.round + ': ' + d.name + ' Grand Prix' +
      (d.isSprint ? '<span class="sprint-badge">SPRINT</span>' : '');
    $('schLoc').textContent = d.location;
    schedSessions = d.sessions || [];
    renderSchedule();
  } catch(e) { $('schName').textContent = 'Failed to load schedule'; }
}

function rowClass(s, idx, nextIdx, now) {
  if (s.utc <= now)  return 's-past';
  if (idx === nextIdx) return 's-next';
  if (s.type === 6)  return 's-gp';      // SESSION_GP
  if (s.type === 4)  return 's-sprint';  // SESSION_SPRINT
  return '';
}

function renderSchedule() {
  const now = Math.floor(Date.now() / 1000);
  const nextIdx = schedSessions.findIndex(s => s.utc > now);
  const tb = $('schBody');
  tb.innerHTML = '';
  schedSessions.forEach((s, i) => {
    const cls  = rowClass(s, i, nextIdx, now);
    const inTxt = (s.utc <= now) ? '&mdash;' : (fmtIn(s.utc) || '&mdash;');
    const row = document.createElement('tr');
    if (cls) row.className = cls;
    row.innerHTML = '<td>' + s.label + '</td><td>' + s.day + '</td><td>' + s.time + '</td>' +
                    '<td class="cd" id="cd' + i + '">' + inTxt + '</td>';
    tb.appendChild(row);
  });
}

function updateCountdowns() {
  const now = Math.floor(Date.now() / 1000);
  schedSessions.forEach((s, i) => {
    const el = $('cd' + i);
    if (!el) return;
    el.textContent = (s.utc <= now) ? '\u2014' : (fmtIn(s.utc) || '\u2014');
  });
}

function fmtGpDate(utcSec) {
  return new Date(utcSec * 1000).toLocaleDateString(undefined,
    {weekday:'short', day:'numeric', month:'short'});
}

async function loadRaces() {
  try {
    const r = await fetch('/api/races');
    const d = await r.json();
    const now = Math.floor(Date.now() / 1000);
    const tb = $('seasonBody');
    tb.innerHTML = '';
    (d.races || []).forEach((race, i) => {
      const done = race.gp < now;
      const cur  = i === 0 && !done;
      const cls  = done ? 'r-done' : cur ? 'r-cur' : '';
      const row  = document.createElement('tr');
      if (cls) row.className = cls;
      const badge = race.isSprint ? '<span class="sprint-badge">S</span>' : '';
      row.innerHTML = '<td>' + race.round + '</td>' +
        '<td>' + race.name + badge + '</td>' +
        '<td>' + race.location + '</td>' +
        '<td style="white-space:nowrap">' + fmtGpDate(race.gp) + '</td>';
      tb.appendChild(row);
    });
  } catch(e) {
    $('seasonBody').innerHTML = '<tr><td colspan="4" style="color:#555">Load failed</td></tr>';
  }
}

renderLogo();
loadConfig();
loadStatus();
loadDebug();
loadSchedule();
loadRaces();
setInterval(loadStatus, 10000);
setInterval(updateCountdowns, 1000);
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

    // Serve F1 logo as raw RGB565 bytes (PROGMEM → browser canvas)
    server.on("/logo.raw", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp = request->beginResponse_P(
            200, "application/octet-stream",
            (const uint8_t*)f1_logo, sizeof(f1_logo));
        resp->addHeader("Cache-Control", "max-age=86400");
        request->send(resp);
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
        DBG_INFO("[Web] Config updated via web UI");
    });

    // GET status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        unsigned long secs = millis() / 1000;
        char uptime[32];
        snprintf(uptime, sizeof(uptime), "%lud %luh %lum",
                 secs / 86400, (secs % 86400) / 3600, (secs % 3600) / 60);
        doc["heap"]   = String(ESP.getFreeHeap() / 1024) + " KB";
        doc["uptime"] = uptime;
        doc["ip"]     = WiFi.localIP().toString();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // GET current race schedule as JSON (for Schedule tab)
    server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest* request) {
        RaceData& race = getCurrentRace();
        JsonDocument doc;
        doc["name"]     = race.name;
        doc["location"] = race.location;
        doc["round"]    = race.round;
        doc["isSprint"] = race.isSprint;
        JsonArray sessions = doc["sessions"].to<JsonArray>();
        for (uint8_t i = 0; i < race.sessionCount; i++) {
            JsonObject s = sessions.add<JsonObject>();
            s["label"] = race.sessions[i].label;
            s["day"]   = race.sessions[i].dayAbbrev;
            s["time"]  = race.sessions[i].localTime;
            s["type"]  = (uint8_t)race.sessions[i].type;
            s["utc"]   = (long)race.sessions[i].utcTime;
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // GET all upcoming races (compact, for season calendar)
    server.on("/api/races", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc["races"].to<JsonArray>();
        for (uint8_t i = 0; i < upcomingCount; i++) {
            JsonObject r = arr.add<JsonObject>();
            r["round"]    = upcomingRaces[i].round;
            r["name"]     = upcomingRaces[i].name;
            r["location"] = upcomingRaces[i].location;
            r["isSprint"] = upcomingRaces[i].isSprint;
            r["gp"]       = (long)upcomingRaces[i].gpTimeUtc;
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // GET debug level
    server.on("/api/debug", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["level"] = debugLevel;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST debug level  (body: level=N  where N is 0-4)
    server.on("/api/debug", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("level", true)) {
            uint8_t newLevel = (uint8_t)request->getParam("level", true)->value().toInt();
            if (newLevel <= DBG_LEVEL_VERBOSE) {
                uint8_t prev = debugLevel;
                debugLevel = newLevel;
                // Use Serial.printf directly — DBG_INFO may be silenced if level was just lowered
                Serial.printf("[INFO] [Web] Debug level: %d → %d\n", prev, debugLevel);
            }
        }
        JsonDocument doc;
        doc["level"] = debugLevel;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

#if SCREENSHOT_WEB_ENABLED
    // POST screenshot capture request (queued for loop execution)
    server.on("/api/screenshot", HTTP_POST, [](AsyncWebServerRequest* request) {
        JsonDocument doc;

        if (!isScreenshotReady()) {
            doc["ok"] = false;
            doc["error"] = "SD not ready";
            String json;
            serializeJson(doc, json);
            request->send(503, "application/json", json);
            return;
        }

        bool queued = requestScreenshot("web");
        doc["ok"] = queued;
        doc["queued"] = queued;
        doc["path"] = getLastScreenshotPath();
        doc["lastError"] = getLastScreenshotError();
        if (!queued) {
            doc["error"] = "Queue failed";
        }
        String json;
        serializeJson(doc, json);
        request->send(queued ? 200 : 500, "application/json", json);
    });
#endif

    // ElegantOTA
    ElegantOTA.begin(&server);

    server.begin();
    DBG_INFO("[Web] Server started on port %d", WEB_SERVER_PORT);
}
