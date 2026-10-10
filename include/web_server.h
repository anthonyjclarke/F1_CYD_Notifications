#pragma once

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_arduino_version.h>
#include "config.h"
#include "types.h"
#include "debug.h"
#include "config_manager.h"
#include "f1_data.h"
#include "f1_logo.h"
#include "time_utils.h"
#include "display_renderer.h"
#include "display_states.h"
#include "screenshot_capture.h"
#include "telegram_handler.h"
#include "timezone_ntp_options.h"

static AsyncWebServer server(WEB_SERVER_PORT);
static AppConfig* _webConfigPtr = nullptr;

// HTML page stored in PROGMEM (avoids LittleFS dependency for core UI)
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>F1 Display</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%23e10600'/%3E%3Ctext x='16' y='22' font-family='Arial' font-weight='900' font-style='italic' font-size='15' fill='white' text-anchor='middle'%3EF1%3C/text%3E%3C/svg%3E">
<style>
:root{--ink:#22272b;--muted:#6b7177;--paper:#f4f3ef;--line:#e2e1db;--rule:#eeede8;--red:#c40500;--brand:#e10600;--soft:#f8f7f3;--edge:#d6d4cc}
*{box-sizing:border-box}
body{margin:0;background:var(--paper);color:var(--ink);font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
main{max-width:960px;margin:auto;padding:40px 32px}
header{display:flex;justify-content:space-between;align-items:center;gap:24px;margin-bottom:8px}
h1{display:flex;align-items:center;gap:14px;font-size:42px;letter-spacing:-1.6px;margin:0;font-weight:650}
h2{font-size:22px;letter-spacing:-0.5px;margin:2px 0 4px}
.dot{color:var(--brand)}
.eyebrow{font-size:11px;font-weight:750;letter-spacing:2px;color:var(--muted);margin:0 0 8px;text-transform:uppercase}
.sub,.muted{color:var(--muted)}
.sub{margin:5px 0 14px}
a{color:var(--red)}
#logo{width:88px;height:48px;flex:0 0 auto;background:#fff;border:1px solid var(--line);border-radius:8px}
button,select,input{font:inherit}
button,.btn{border:1px solid var(--red);background:var(--red);color:#fff;padding:9px 15px;border-radius:7px;cursor:pointer;white-space:nowrap;text-decoration:none;display:inline-block;line-height:1.5}
button:hover,.btn:hover{filter:brightness(.94)}
.secondary{background:transparent;color:var(--red);border-color:var(--edge)}
.actions{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
nav .tab{background:none;border:0;padding:4px 2px;color:var(--red);text-decoration:underline;font-size:13px}
nav .tab.active{color:var(--ink);text-decoration:none;font-weight:650}
label{display:block;font-size:12px;color:var(--muted)}
input,select{display:block;width:100%;border:1px solid var(--edge);border-radius:6px;padding:8px 10px;background:#fff;color:var(--ink);margin:4px 0 0}
input:focus-visible,select:focus-visible,button:focus-visible{outline:2px solid var(--red);outline-offset:1px}
input[type=range]{padding:0;border:0;background:none;accent-color:var(--red)}
.badge{border-left:4px solid #6f8f7a;border-radius:6px;background:#eaf1e9;padding:12px 16px;margin:16px 0;font-size:14px}
.badge.busy{border-color:#b28432;background:#fff6dc}
.badge.warning{border-color:#a45846;background:#fff0e9}
.badge small{display:block;margin-top:4px;color:var(--muted);font-size:12px}
.msg{display:none;position:sticky;top:12px;z-index:10;border-radius:7px;padding:10px 15px;margin:0 0 16px;font-size:13px;box-shadow:0 6px 18px rgba(0,0,0,.08)}
.msg.ok{display:block;background:#eaf1e9;border:1px solid #d3dfd0}
.msg.err{display:block;background:#fff0e9;color:#963e27;border:1px solid #f1d6ca}
.msg a{color:inherit;font-weight:600}
.cards{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px;margin:20px 0}
.card{border-top:3px solid var(--brand);background:#fff;padding:18px 20px;border-radius:8px}
.card-label{font-size:13px}
.card strong{display:block;font-size:28px;font-weight:600;letter-spacing:-0.5px;margin:6px 0;font-variant-numeric:tabular-nums}
.card small{color:var(--muted);font-size:12px}
.panel{background:#fff;border:1px solid var(--line);border-radius:10px;padding:24px;margin:22px 0}
.footnote{font-size:12px;color:var(--muted);margin:12px 0 0}
.table-wrap{overflow:auto}
table{border-collapse:collapse;width:100%;font-size:13px}
th,td{text-align:left;padding:10px 12px;border-bottom:1px solid var(--rule)}
th{font-weight:600;color:var(--muted);background:var(--soft);font-size:12px}
tbody tr:hover{background:#fbfaf7}
th.r,td.cd{text-align:right}
td.cd{font-variant-numeric:tabular-nums;color:var(--muted);white-space:nowrap}
.s-past td{color:#a9adb0}
.s-next td{background:#fdf1ef;font-weight:600}
.s-next td.cd{color:var(--red)}
.s-gp td:first-child{color:var(--red);font-weight:650}
.s-sprint td:first-child{color:#94650a;font-weight:650}
.r-cur td{font-weight:650}
.r-cur td:first-child{box-shadow:inset 3px 0 var(--brand)}
.r-done td{color:#a9adb0}
.pill{display:inline-block;background:#fff3cd;color:#7a5a00;font-size:10.5px;font-weight:700;letter-spacing:.5px;padding:1px 7px;border-radius:999px;margin-left:8px;vertical-align:2px}
.post-race{border-left:4px solid var(--brand)}
.p-row{display:flex;align-items:baseline;gap:12px;padding:8px 0;border-bottom:1px solid var(--rule);font-size:14px}
.p-row:last-child{border-bottom:0}
.p-pos{min-width:34px;font-weight:700}
.p-name{flex:1}
.p-team{color:var(--muted);font-size:12px;text-align:right}
.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.range-wrap{display:flex;align-items:center;gap:12px;margin-top:6px}
.range-wrap input{flex:1;margin:0}
.range-val{min-width:44px;text-align:right;font-weight:650;font-variant-numeric:tabular-nums}
.diag-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px 20px;margin-top:14px;align-items:start}
.kv td{overflow-wrap:anywhere;padding:7px 10px}
.kv td:first-child{color:var(--muted);width:40%}
.tool-row{display:flex;justify-content:space-between;align-items:center;gap:16px;padding:14px 0;border-bottom:1px solid var(--rule)}
.tool-row:last-child{border-bottom:0;padding-bottom:0}
.tool-row small{display:block;color:var(--muted);font-size:12px}
.tool-row select{width:auto;margin:0}
footer{font-size:12px;color:var(--muted);margin:30px 0;line-height:1.9}
footer a{color:var(--muted)}
[hidden]{display:none!important}
@media (max-width:800px){
  main{padding:24px 16px}
  header{flex-direction:column;align-items:flex-start}
  h1{font-size:34px}
  .cards{grid-template-columns:1fr 1fr}
  .panel{padding:17px}
  .form-grid{grid-template-columns:1fr}
}
@media (max-width:480px){
  .cards{grid-template-columns:1fr}
  th,td{padding:8px}
  .tool-row{flex-direction:column;align-items:flex-start}
}
</style>
</head>
<body>
<main>
<header>
  <div>
    <p class="eyebrow">F1 CYD &middot; Notifications</p>
    <h1><canvas id="logo" width="118" height="64"></canvas><span>F1 Display<span class="dot">.</span></span></h1>
    <p class="sub" style="margin-bottom:0">Race schedule, countdowns and Telegram alerts.</p>
  </div>
  <nav class="actions">
    <button type="button" class="tab active" id="btn-sch" onclick="showTab('sch')">Schedule</button>
    <button type="button" class="tab" id="btn-cfg" onclick="showTab('cfg')">Settings</button>
    <button type="button" class="tab" id="btn-diag" onclick="showTab('diag')">System / Diagnostics</button>
    <button type="button" onclick="refreshAll()">&#8635; Refresh</button>
  </nav>
</header>

<div id="status" class="badge busy" role="status"><span id="statusMain">Connecting&hellip;</span><small id="statusSub">&nbsp;</small></div>
<div class="msg" id="msg" role="alert"></div>

<!-- Schedule Tab -->
<div id="tab-sch">
<section class="cards">
  <div class="card"><div class="card-label">Next session</div><strong id="cNext">&ndash;</strong><small id="cNextSub">&nbsp;</small></div>
  <div class="card"><div class="card-label">Grand Prix</div><strong id="cGp">&ndash;</strong><small id="cGpSub">&nbsp;</small></div>
  <div class="card"><div class="card-label">Season</div><strong id="cSeason">&ndash;</strong><small id="cSeasonSub">&nbsp;</small></div>
</section>

<section id="schPrev" class="panel post-race" hidden></section>

<section class="panel">
  <p class="eyebrow" id="schEyebrow">This weekend</p>
  <h2 id="schName">Loading&hellip;</h2>
  <p class="sub" id="schLoc"></p>
  <div class="table-wrap"><table>
    <thead><tr><th>Session</th><th>Day</th><th>Time</th><th class="r">Starts in</th></tr></thead>
    <tbody id="schBody"></tbody>
  </table></div>
  <p class="footnote">Times are in the device timezone (<span id="schTz">-</span>).</p>
</section>

<section class="panel">
  <p class="eyebrow">2026 season</p>
  <h2>Calendar</h2>
  <div class="table-wrap"><table id="seasonTable">
    <thead><tr><th>Rd</th><th>Grand Prix</th><th>Location</th><th>Date</th></tr></thead>
    <tbody id="seasonBody"><tr><td colspan="4" class="muted">Loading&hellip;</td></tr></tbody>
  </table></div>
  <p class="footnote">Dates are in this browser's timezone.</p>
</section>
</div>

<!-- Settings Tab -->
<div id="tab-cfg" hidden>
<form id="configForm">
<section class="panel">
  <p class="eyebrow">Time</p>
  <h2>Timezone &amp; NTP</h2>
  <p class="sub">Session times on the display and here follow this timezone.</p>
  <div class="form-grid">
    <label>Timezone<select id="tz" name="tz"></select></label>
    <label>NTP server<select id="ntp" name="ntp"></select></label>
  </div>
</section>

<section class="panel">
  <p class="eyebrow">Display</p>
  <h2>Brightness</h2>
  <label for="bright">Backlight &ndash; 0 follows the light sensor</label>
  <div class="range-wrap">
    <input type="range" id="bright" name="bright" min="0" max="255" value="128">
    <span class="range-val" id="brightVal">128</span>
  </div>
  <p class="footnote">Light sensor reading: <span id="ldrVal">-</span> / 4095</p>
</section>

<section class="panel">
  <p class="eyebrow">Notifications</p>
  <h2>Telegram</h2>
  <p class="sub">Race week, session reminders and results. A confirmation is sent when these change.</p>
  <div class="form-grid">
    <label>Bot token<input type="text" id="bot" name="bot" placeholder="123456:ABC-DEF..." autocomplete="off"></label>
    <label>Chat ID<input type="text" id="chat" name="chat" placeholder="123456789" autocomplete="off"></label>
  </div>
  <div class="actions" style="margin-top:16px">
    <button type="button" class="secondary" onclick="telegramAction('test')">Send test</button>
    <button type="button" class="secondary" onclick="telegramAction('resend')">Resend last</button>
  </div>
</section>

<div class="actions"><button type="submit">Save settings</button></div>
</form>
</div>

<!-- System / Diagnostics Tab -->
<div id="tab-diag" hidden>
<section class="panel">
  <p class="eyebrow">Device</p>
  <h2>Hardware &amp; diagnostics</h2>
  <div class="diag-grid">
    <table class="kv"><thead><tr><th colspan="2">Firmware</th></tr></thead><tbody>
      <tr><td>Version</td><td id="dFw">-</td></tr>
      <tr><td>Built</td><td id="dBuilt">-</td></tr>
      <tr><td>Partition</td><td id="dPart">-</td></tr>
      <tr><td>Core / IDF</td><td id="dCore">-</td></tr>
    </tbody></table>
    <table class="kv"><thead><tr><th colspan="2">Network &amp; time</th></tr></thead><tbody>
      <tr><td>WiFi</td><td id="dWifi">-</td></tr>
      <tr><td>IP</td><td id="dIp">-</td></tr>
      <tr><td>NTP</td><td id="dNtp">-</td></tr>
      <tr><td>Local time</td><td id="dTime">-</td></tr>
    </tbody></table>
    <table class="kv"><thead><tr><th colspan="2">Hardware</th></tr></thead><tbody>
      <tr><td>Board</td><td id="dBoard">-</td></tr>
      <tr><td>Chip</td><td id="dChip">-</td></tr>
      <tr><td>Flash</td><td id="dFlash">-</td></tr>
      <tr><td>Device</td><td id="dDev">-</td></tr>
      <tr><td>MAC</td><td id="dMac">-</td></tr>
      <tr><td>SD card</td><td id="dSd">-</td></tr>
    </tbody></table>
    <table class="kv"><thead><tr><th colspan="2">System</th></tr></thead><tbody>
      <tr><td>Uptime</td><td id="uptime">-</td></tr>
      <tr><td>Last reset</td><td id="dReset">-</td></tr>
      <tr><td>Free heap</td><td id="heap">-</td></tr>
      <tr><td>Heap low / block</td><td id="dHeapLow">-</td></tr>
      <tr><td>LittleFS</td><td id="dFs">-</td></tr>
    </tbody></table>
    <table class="kv"><thead><tr><th colspan="2">F1 data</th></tr></thead><tbody>
      <tr><td>Current race</td><td id="dRace">-</td></tr>
      <tr><td>Results cached</td><td id="dResults">-</td></tr>
    </tbody></table>
  </div>
</section>

<section class="panel">
  <p class="eyebrow">Tools</p>
  <h2>Maintenance</h2>
  <div class="tool-row">
    <div><strong>Firmware update</strong><small>Upload a *-firmware.bin over the network.</small></div>
    <a class="btn secondary" href="/update">Open OTA update</a>
  </div>
  <div class="tool-row">
    <div><strong>TFT screenshot</strong><small>Saved to the SD card, or held in RAM for download.</small></div>
    <button type="button" class="secondary" onclick="captureShot()">Capture screenshot</button>
  </div>
  <div class="tool-row">
    <div><strong>Serial debug level</strong><small>Runtime only &ndash; resets on reboot.</small></div>
    <div class="actions">
      <select id="dbg">
        <option value="0">0 - Off</option>
        <option value="1">1 - Error</option>
        <option value="2">2 - Warn</option>
        <option value="3">3 - Info (default)</option>
        <option value="4">4 - Verbose</option>
      </select>
      <button type="button" class="secondary" onclick="setDbg()">Apply</button>
    </div>
  </div>
</section>
</div>

<footer>
  <div><a href="https://github.com/anthonyjclarke/F1_CYD_Notifications" target="_blank" rel="noopener">GitHub</a> &middot; <a href="https://bsky.app/profile/anthonyjclarke.bsky.social" target="_blank" rel="noopener">Bluesky</a> &middot; Built with &#10084; by Anthony Clarke</div>
  <div>Based on an original idea by <a href="https://github.com/witnessmenow/F1-Arduino-Notifications" target="_blank" rel="noopener">@witnessmenow</a> &middot; Data: <a href="https://github.com/sportstimes/f1" target="_blank" rel="noopener">sportstimes/f1</a> &amp; <a href="https://github.com/jolpica/jolpica-f1" target="_blank" rel="noopener">Jolpica F1 API</a></div>
</footer>
</main>

<script>
const $ = id => document.getElementById(id);
const bright = $('bright');
const brightLabel = v => v == 0 ? 'Auto' : v;
bright.oninput = () => $('brightVal').textContent = brightLabel(bright.value);

const TABS = ['sch', 'cfg', 'diag'];
function showTab(t) {
  if (!TABS.includes(t)) t = 'sch';
  TABS.forEach(k => {
    $('tab-' + k).hidden = k !== t;
    $('btn-' + k).classList.toggle('active', k === t);
  });
  history.replaceState(null, '', '#' + t);
}

function refreshAll() {
  loadStatus();
  loadSchedule();
  loadRaces();
}

async function renderLogo() {
  try {
    const r = await fetch('/logo.raw');
    if (!r.ok) throw new Error(r.status);
    const buf = await r.arrayBuffer();
    const px = new Uint16Array(buf);
    const c = $('logo');
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
  } catch(e) { $('logo').hidden = true; }
}

async function loadConfig() {
  try {
    const r = await fetch('/api/config');
    const c = await r.json();
    $('tz').value   = c.tz     || 'Europe/London';
    $('ntp').value  = c.ntp    || 'pool.ntp.org';
    $('bot').value  = c.bot    || '';
    $('chat').value = c.chat   || '';
    bright.value    = c.bright ?? 128;   // 0 = auto, keep it
    $('brightVal').textContent = brightLabel(bright.value);
  } catch(e) { console.error(e); }
}

async function loadStatus() {
  try {
    const r = await fetch('/api/status');
    const s = await r.json();
    $('heap').textContent   = s.heap   || '-';
    $('uptime').textContent = s.uptime || '-';
    if (s.ldr !== undefined) $('ldrVal').textContent = s.ldr;
    const kb = b => Math.round(b / 1024) + ' KB';
    const rssiQ = r => r >= -60 ? 'good' : r >= -70 ? 'fair' : 'weak';
    $('dFw').textContent    = 'v' + s.fw;
    $('dBuilt').textContent = s.built;
    $('dPart').textContent  = s.part;
    $('dCore').textContent  = 'Arduino ' + s.core + ' / ' + s.sdk;
    $('dBoard').textContent = s.board;
    $('dChip').textContent  = s.chip + ' rev ' + s.chipRev + ' · ' + s.cores + ' cores @ ' + s.cpuMhz + ' MHz';
    $('dFlash').textContent = (s.flash / 1048576) + ' MB';
    $('dDev').textContent   = s.device;
    $('dMac').textContent   = s.mac;
    $('dSd').textContent    = s.sd ? 'Ready' : 'Not detected';
    $('dWifi').textContent  = s.ssid + ' · ' + s.rssi + ' dBm (' + rssiQ(s.rssi) + ')';
    $('dIp').textContent    = s.ip;
    let ntpText;
    if (s.ntpSynced) {
      const ago = Math.max(0, Math.round((s.now - s.ntpLast) / 60));
      ntpText = 'Synced ' + (ago < 1 ? 'just now' : ago + ' min ago') + ' · ' + s.ntpServer;
    } else {
      ntpText = 'Not synced – using saved time';
    }
    $('dNtp').textContent   = ntpText;
    $('dTime').textContent  = s.localTime + ' (' + s.tz + ')';
    $('schTz').textContent  = s.tz;
    $('dReset').textContent = s.reset;
    $('dHeapLow').textContent = kb(s.heapMin) + ' / ' + kb(s.heapMaxBlock);
    $('dFs').textContent    = kb(s.fsUsed) + ' of ' + kb(s.fsTotal) + ' used';
    $('dRace').textContent  = s.raceRound ? 'R' + s.raceRound + ' ' + s.raceName : 'No schedule';
    $('dResults').textContent = s.resultsRound ? 'R' + s.resultsRound : 'None';

    $('status').className = s.ntpSynced ? 'badge' : 'badge busy';
    $('statusMain').textContent = '✓ Connected · ' + s.device + ' · ' + s.ip + ' · v' + s.fw;
    $('statusSub').textContent  = 'NTP: ' + ntpText + ' · Device time ' + s.localTime;
  } catch(e) {
    $('status').className = 'badge warning';
    $('statusMain').textContent = 'Connection lost';
    $('statusSub').textContent  = 'Retrying every 10 seconds.';
  }
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

// Sends run on the device's main loop; poll until it reports the result
async function waitTelegram(msg, okText, failText) {
  msg.className = 'msg ok';
  msg.textContent = 'Sending Telegram message…';
  for (let i = 0; i < 120; i++) {   // up to 30 s
    await new Promise(r => setTimeout(r, 250));
    try {
      const st = await (await fetch('/api/telegram/status')).json();
      if (!st.pending) {
        const ok = st.lastResult === 1;
        msg.className = ok ? 'msg ok' : 'msg err';
        msg.textContent = ok ? okText : failText;
        return;
      }
    } catch(_) {}
  }
  msg.className = 'msg err';
  msg.textContent = 'Telegram: no result yet – check the device log';
}

async function telegramAction(action) {
  const msg = $('msg');
  try {
    const r = await fetch('/api/telegram/' + action, {method:'POST'});
    const d = await r.json();
    if (r.ok && d.queued) {
      await waitTelegram(msg,
        action === 'test' ? 'Telegram test sent' : 'Last Telegram message resent',
        'Telegram send failed');
    } else {
      msg.className = 'msg err';
      msg.textContent = d.message || 'Telegram send failed';
    }
  } catch(e) {
    msg.className = 'msg err';
    msg.textContent = 'Telegram error: ' + e.message;
  }
  setTimeout(() => msg.className = 'msg', 8000);
}

async function captureShot() {
  const msg = $('msg');
  try {
    const r = await fetch('/api/screenshot', {method:'POST'});
    const d = await r.json();
    msg.className = r.ok ? 'msg ok' : 'msg err';
    if (r.ok && d.ram) {
      // No SD card — captured to RAM; poll then show download link
      msg.innerHTML = 'Capturing to RAM&hellip; <a id="dlLink" hidden href="/api/screenshot/download?ram=1" download="screenshot.bmp">Download</a>';
      let attempts = 0;
      const checkReady = async () => {
        attempts++;
        try {
          const chk = await fetch('/api/screenshot/status');
          const st = await chk.json();
          if (st.ram_ready) {
            const link = $('dlLink');
            if (link) link.hidden = false;
            return;
          }
        } catch(_) {}
        if (attempts < 40) setTimeout(checkReady, 250);
      };
      setTimeout(checkReady, 200);
    } else if (r.ok && d.queued) {
      // SD card path — poll until capture completes, then show download link
      msg.textContent = 'Capturing…';
      let attempts = 0;
      const checkDone = async () => {
        attempts++;
        try {
          const chk = await fetch('/api/screenshot/status');
          const st = await chk.json();
          if (!st.busy && st.lastPath) {
            const filename = st.lastPath.split('/').pop();
            const url = `/api/screenshot/download?file=${encodeURIComponent(filename)}`;
            msg.innerHTML = `Screenshot saved: <a href="${url}" target="_blank">${st.lastPath}</a>`;
            return;
          } else if (!st.busy && st.lastError) {
            msg.className = 'msg err';
            msg.textContent = 'Screenshot failed: ' + st.lastError;
            return;
          }
        } catch(_) {}
        if (attempts < 40) setTimeout(checkDone, 250);
        else msg.textContent = 'Screenshot timed out';
      };
      setTimeout(checkDone, 200);
    } else {
      msg.textContent = d.error || 'Screenshot failed';
    }
  } catch(e) {
    msg.className = 'msg err';
    msg.textContent = 'Screenshot error: ' + e.message;
  }
  setTimeout(() => msg.className = 'msg', 12000);
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
    const d = await r.json().catch(() => ({}));
    msg.className   = r.ok ? 'msg ok'  : 'msg err';
    if (r.ok && d.telegramTestQueued) {
      await waitTelegram(msg,
        'Settings saved. Telegram confirmation sent.',
        'Settings saved. Telegram confirmation failed.');
    } else if (r.ok && d.telegramConfigured && d.telegramChanged) {
      msg.className = 'msg err';
      msg.textContent = 'Settings saved. Telegram confirmation not sent (another send in progress).';
    } else {
      msg.textContent = r.ok ? 'Settings saved' : 'Save failed';
    }
  } catch(e) {
    msg.className = 'msg err'; msg.textContent = 'Error: ' + e.message;
  }
  setTimeout(() => msg.className = 'msg', 8000);
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
    const prevDiv = $('schPrev');
    const postRace = d.postRace && d.next;

    if (postRace) {
      // Post-race mode: podium card for the finished race, next race below
      const posColors = ['#b8860b','#7d8590','#a0612b','#6b7177','#6b7177'];
      const posLabels = ['1st','2nd','3rd','4th','5th'];
      let podHtml;
      if (d.resultsAvailable && d.podium && d.podium.length) {
        podHtml = d.podium.map((p,i) =>
          '<div class="p-row">' +
          '<span class="p-pos" style="color:' + posColors[i] + '">' + posLabels[i] + '</span>' +
          '<span class="p-name">' + p.name + '</span>' +
          '<span class="p-team">' + p.team + '</span>' +
          '</div>').join('');
      } else {
        podHtml = '<p class="muted" style="margin:0">Results not yet available.</p>';
      }
      prevDiv.innerHTML = '<p class="eyebrow">&#10003; Post-race &middot; Round ' + d.round + '</p>' +
        '<h2 style="margin-bottom:10px">' + d.name + ' Grand Prix</h2>' + podHtml;
    }
    prevDiv.hidden = !postRace;

    const race = postRace ? d.next : d;
    $('schEyebrow').textContent = 'Round ' + race.round + (postRace ? ' · Up next' : ' · This race');
    $('schName').innerHTML = race.name + ' Grand Prix' +
      (race.isSprint ? '<span class="pill">SPRINT</span>' : '');
    $('schLoc').textContent = race.location;
    schedSessions = race.sessions || [];

    const gp = schedSessions.find(s => s.type === 6);   // SESSION_GP
    $('cGp').textContent    = gp ? gp.day + ' ' + gp.time : '–';
    $('cGpSub').textContent = 'R' + race.round + ' · ' + race.name + ' · ' + race.location;
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
  updateNextCard();
}

function updateNextCard() {
  const nx = schedSessions.find(s => s.utc > Math.floor(Date.now() / 1000));
  $('cNext').textContent    = nx ? fmtIn(nx.utc) : 'Done';
  $('cNextSub').textContent = nx ? nx.label + ' · ' + nx.day + ' ' + nx.time
                                 : 'All sessions this weekend have run';
}

function updateCountdowns() {
  const now = Math.floor(Date.now() / 1000);
  schedSessions.forEach((s, i) => {
    const el = $('cd' + i);
    if (!el) return;
    el.textContent = (s.utc <= now) ? '—' : (fmtIn(s.utc) || '—');
  });
  updateNextCard();
}

function fmtGpDate(utcSec) {
  return new Date(utcSec * 1000).toLocaleDateString(undefined,
    {weekday:'short', day:'numeric', month:'short'});
}

async function loadRaces() {
  try {
    const r = await fetch('/api/races');
    const d = await r.json();
    const races = d.races || [];
    const now = Math.floor(Date.now() / 1000);
    const tb = $('seasonBody');
    tb.innerHTML = '';
    races.forEach((race, i) => {
      const done = race.gp < now;
      const cur  = i === 0 && !done;
      const cls  = done ? 'r-done' : cur ? 'r-cur' : '';
      const row  = document.createElement('tr');
      if (cls) row.className = cls;
      const badge = race.isSprint ? '<span class="pill">SPRINT</span>' : '';
      row.innerHTML = '<td>' + race.round + '</td>' +
        '<td>' + race.name + badge + '</td>' +
        '<td>' + race.location + '</td>' +
        '<td style="white-space:nowrap">' + fmtGpDate(race.gp) + '</td>';
      tb.appendChild(row);
    });
    const left = races.filter(x => x.gp > now).length;
    const last = races[races.length - 1];
    $('cSeason').textContent    = left ? left + (left === 1 ? ' race' : ' races') + ' left' : 'Complete';
    $('cSeasonSub').textContent = last ? 'Finale: ' + last.name + ' · ' + fmtGpDate(last.gp) : '';
  } catch(e) {
    $('seasonBody').innerHTML = '<tr><td colspan="4" class="muted">Load failed</td></tr>';
  }
}

// Populate timezone and NTP server select options
function populateSelects() {
  const tzOpts = [
    'UTC',
    'Europe/London','Europe/Paris','Europe/Amsterdam','Europe/Berlin','Europe/Rome','Europe/Madrid',
    'Europe/Zurich','Europe/Vienna','Europe/Brussels','Europe/Prague','Europe/Warsaw','Europe/Moscow','Europe/Istanbul',
    'America/New_York','America/Toronto','America/Chicago','America/Denver','America/Los_Angeles','America/Anchorage',
    'America/Mexico_City','America/Bogota','America/Buenos_Aires','America/Sao_Paulo',
    'Asia/Dubai','Asia/Bangkok','Asia/Hong_Kong','Asia/Shanghai','Asia/Tokyo','Asia/Seoul','Asia/Singapore','Asia/Kolkata',
    'Australia/Sydney','Australia/Melbourne','Australia/Brisbane','Australia/Perth',
    'Africa/Johannesburg','Africa/Cairo','Africa/Lagos',
    'GMT-12','GMT-11','GMT-10','GMT-9','GMT-8','GMT-7','GMT-6','GMT-5','GMT-4','GMT-3','GMT-2','GMT-1',
    'GMT+0','GMT+1','GMT+2','GMT+3','GMT+4','GMT+5','GMT+6','GMT+7','GMT+8','GMT+9','GMT+10','GMT+11','GMT+12','GMT+13','GMT+14'
  ];
  const ntpOpts = [
    'pool.ntp.org','time.nist.gov','time.google.com','time.cloudflare.com',
    '0.pool.ntp.org','1.pool.ntp.org','2.pool.ntp.org','3.pool.ntp.org',
    'time.apple.com','time.windows.com','ntp.ubuntu.com','0.amazon.pool.ntp.org',
    'time1.google.com','time2.google.com'
  ];

  const tzSel = $('tz');
  tzOpts.forEach(o => {
    const opt = document.createElement('option');
    opt.value = o;
    opt.textContent = o;
    tzSel.appendChild(opt);
  });

  const ntpSel = $('ntp');
  ntpOpts.forEach(o => {
    const opt = document.createElement('option');
    opt.value = o;
    opt.textContent = o;
    ntpSel.appendChild(opt);
  });
}

showTab(location.hash.slice(1));
renderLogo();
populateSelects();
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

// Plain-English reason for the last reset (Hardware & Diagnostics panel)
static const char* resetReasonText(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "Power on";
        case ESP_RST_EXT:       return "External reset";
        case ESP_RST_SW:        return "Software restart";
        case ESP_RST_PANIC:     return "Crash (panic)";
        case ESP_RST_INT_WDT:   return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "Task watchdog";
        case ESP_RST_WDT:       return "Watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
        case ESP_RST_BROWNOUT:  return "Brownout";
        case ESP_RST_SDIO:      return "SDIO reset";
        default:                return "Unknown";
    }
}

// Timezone / NTP server changed by POST /api/config; applied by loop() so the
// handler never blocks the AsyncTCP task (initTime() could wait 15 s for NTP).
static volatile bool timeConfigChanged = false;

void applyTimeConfigChange() {
    if (!timeConfigChanged || !_webConfigPtr) return;
    timeConfigChanged = false;
    setServer(_webConfigPtr->ntpServer);
    applyTimezone(_webConfigPtr->timezone);
    for (uint8_t i = 0; i < MAX_RACES; i++) {
        updateSessionTimesForRace(races[i]);
    }
    requestRedraw();
}

// Queue a Telegram request for loop() and reply 202, or 400 with reason / 409 if busy
static void sendTelegramQueueResponse(AsyncWebServerRequest* request, bool allowed,
                                      TelegramRequest req, const char* notAllowedMsg) {
    JsonDocument doc;
    int code;
    if (!allowed) {
        code = 400;
        doc["message"] = notAllowedMsg;
    } else if (!queueTelegramRequest(req)) {
        code = 409;
        doc["message"] = "A Telegram send is already in progress";
    } else {
        code = 202;
        doc["queued"] = true;
    }
    String json;
    serializeJson(doc, json);
    request->send(code, "application/json", json);
}

void setupWebServer(AppConfig& cfg) {
    _webConfigPtr = &cfg;

    // Serve config page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", INDEX_HTML);
    });

    // Serve F1 logo as raw RGB565 bytes (PROGMEM → browser canvas)
    server.on("/logo.raw", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp = request->beginResponse(
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

        char oldTimezone[sizeof(_webConfigPtr->timezone)];
        char oldNtpServer[sizeof(_webConfigPtr->ntpServer)];
        strlcpy(oldTimezone, _webConfigPtr->timezone, sizeof(oldTimezone));
        strlcpy(oldNtpServer, _webConfigPtr->ntpServer, sizeof(oldNtpServer));
        char oldBotToken[sizeof(_webConfigPtr->botToken)];
        char oldChatId[sizeof(_webConfigPtr->chatId)];
        strlcpy(oldBotToken, _webConfigPtr->botToken, sizeof(oldBotToken));
        strlcpy(oldChatId, _webConfigPtr->chatId, sizeof(oldChatId));

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

        _webConfigPtr->telegramEnabled =
            strlen(_webConfigPtr->botToken) > 0 && strlen(_webConfigPtr->chatId) > 0;
        bool telegramChanged =
            strcmp(oldBotToken, _webConfigPtr->botToken) != 0 ||
            strcmp(oldChatId, _webConfigPtr->chatId) != 0;

        // Apply brightness immediately
        updateBrightness(_webConfigPtr->brightness);

        // Timezone / NTP server: applied by loop(), which also redraws with the new times
        if (strcmp(oldTimezone, _webConfigPtr->timezone) != 0 ||
            strcmp(oldNtpServer, _webConfigPtr->ntpServer) != 0) {
            timeConfigChanged = true;
        }

        saveConfig(*_webConfigPtr);
        // Confirmation message is sent from loop(); the page polls /api/telegram/status
        bool telegramTestQueued = telegramChanged && _webConfigPtr->telegramEnabled &&
                                  queueTelegramRequest(TG_REQ_TEST);

        JsonDocument resp;
        resp["ok"] = true;
        resp["telegramConfigured"] = _webConfigPtr->telegramEnabled;
        resp["telegramChanged"] = telegramChanged;
        resp["telegramTestQueued"] = telegramTestQueued;
        String json;
        serializeJson(resp, json);
        request->send(200, "application/json", json);
        DBG_INFO("[Web] Config updated via web UI");

    });

    // GET Telegram status
    server.on("/api/telegram/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!_webConfigPtr) { request->send(500); return; }
        JsonDocument doc;
        doc["configured"] = _webConfigPtr->telegramEnabled;
        doc["hasLastMessage"] = telegramHasLastMessage();
        doc["pending"] = telegramRequestPending();
        doc["lastResult"] = telegramLastResult;  // -1 none yet, 0 failed, 1 sent
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST Telegram test message / resend the last successfully sent message.
    // Queued for loop(); the page polls /api/telegram/status for the result.
    server.on("/api/telegram/test", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!_webConfigPtr) { request->send(500); return; }
        sendTelegramQueueResponse(request, _webConfigPtr->telegramEnabled, TG_REQ_TEST,
                                  "Telegram is not configured");
    });

    server.on("/api/telegram/resend", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!_webConfigPtr) { request->send(500); return; }
        sendTelegramQueueResponse(request,
                                  _webConfigPtr->telegramEnabled && telegramHasLastMessage(),
                                  TG_REQ_RESEND, "No saved Telegram message to resend");
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
        doc["ldr"]    = analogRead(PIN_LDR);

        // Hardware & Diagnostics panel
        doc["fw"]      = FIRMWARE_VERSION;
        doc["built"]   = __DATE__ " " __TIME__;
        doc["part"]    = esp_ota_get_running_partition()->label;
        char core[16];
        snprintf(core, sizeof(core), "%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR,
                 ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
        doc["core"]    = core;
        doc["sdk"]     = ESP.getSdkVersion();
        doc["board"]   = BOARD_NAME;
        doc["chip"]    = ESP.getChipModel();
        doc["chipRev"] = ESP.getChipRevision();
        doc["cores"]   = ESP.getChipCores();
        doc["cpuMhz"]  = ESP.getCpuFreqMHz();
        doc["flash"]   = ESP.getFlashChipSize();
        char device[16];  // Same name the web installer shows (improv_setup.cpp)
        snprintf(device, sizeof(device), IMPROV_DEVICE_PREFIX "-%04X",
                 (unsigned)(ESP.getEfuseMac() & 0xFFFF));
        doc["device"]  = device;
        doc["mac"]     = WiFi.macAddress();
        doc["sd"]      = screenshotSdReady;
        doc["ssid"]    = WiFi.SSID();
        doc["rssi"]    = WiFi.RSSI();
        doc["ntpSynced"] = ntpHasSynced();
        doc["ntpLast"] = (long)lastNtpUpdateTime();
        doc["now"]     = (long)nowUTC();
        doc["ntpServer"] = _webConfigPtr ? _webConfigPtr->ntpServer : "";
        doc["tz"]      = _webConfigPtr ? _webConfigPtr->timezone : "";
        char localTime[48];
        formatLocalFullDate(nowUTC(), localTime, sizeof(localTime));
        doc["localTime"] = localTime;
        doc["reset"]   = resetReasonText(esp_reset_reason());
        doc["heapMin"] = ESP.getMinFreeHeap();
        doc["heapMaxBlock"] = ESP.getMaxAllocHeap();
        doc["fsUsed"]  = LittleFS.usedBytes();
        doc["fsTotal"] = LittleFS.totalBytes();
        RaceData& race = getCurrentRace();
        doc["raceRound"] = race.round;
        doc["raceName"]  = race.name;
        doc["resultsRound"] = resultsRound;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // GET current race schedule as JSON (for Schedule tab)
    // In post-race window: includes postRace=true, podium[], resultsAvailable, next{} object
    server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest* request) {
        RaceData& race = getCurrentRace();
        time_t now = nowUTC();

        // Detect post-race window
        bool inPostRace = (race.gpTimeUtc > 0 &&
                           now >= race.gpTimeUtc &&
                           (now - race.gpTimeUtc) < (POST_RACE_DAYS * 86400L));

        JsonDocument doc;
        doc["name"]             = race.name;
        doc["location"]         = race.location;
        doc["round"]            = race.round;
        doc["isSprint"]         = race.isSprint;
        doc["postRace"]         = inPostRace;
        doc["resultsAvailable"] = hasResultsFor(race);

        // Current race sessions (always included — useful even in post-race for reference)
        JsonArray sessions = doc["sessions"].to<JsonArray>();
        for (uint8_t i = 0; i < race.sessionCount; i++) {
            JsonObject s = sessions.add<JsonObject>();
            s["label"] = race.sessions[i].label;
            s["day"]   = race.sessions[i].dayAbbrev;
            s["time"]  = race.sessions[i].localTime;
            s["type"]  = (uint8_t)race.sessions[i].type;
            s["utc"]   = (long)race.sessions[i].utcTime;
        }

        // Podium results (if available)
        if (hasResultsFor(race)) {
            JsonArray pod = doc["podium"].to<JsonArray>();
            for (uint8_t i = 0; i < podiumCount; i++) {
                JsonObject p = pod.add<JsonObject>();
                p["pos"]  = podium[i].position;
                p["name"] = podium[i].driverName;
                p["team"] = podium[i].constructor;
            }
        }

        // Next race sessions (only when in post-race window and next race differs)
        if (inPostRace && races[2].round != race.round && races[2].sessionCount > 0) {
            RaceData& next = races[2];
            JsonObject nextObj = doc["next"].to<JsonObject>();
            nextObj["name"]     = next.name;
            nextObj["location"] = next.location;
            nextObj["round"]    = next.round;
            nextObj["isSprint"] = next.isSprint;
            JsonArray ns = nextObj["sessions"].to<JsonArray>();
            for (uint8_t i = 0; i < next.sessionCount; i++) {
                JsonObject s = ns.add<JsonObject>();
                s["label"] = next.sessions[i].label;
                s["day"]   = next.sessions[i].dayAbbrev;
                s["time"]  = next.sessions[i].localTime;
                s["type"]  = (uint8_t)next.sessions[i].type;
                s["utc"]   = (long)next.sessions[i].utcTime;
            }
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
    // GET screenshot capture status (for client polling)
    server.on("/api/screenshot/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["sd_ready"]  = isScreenshotReady();
        doc["ram_ready"] = isScreenshotRamReady();
        doc["busy"]      = isScreenshotBusy();
        doc["lastPath"]  = getLastScreenshotPath();
        doc["lastError"] = getLastScreenshotError();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // GET screenshot download — ?file=<name> for SD card, ?ram=1 for RAM capture
    server.on("/api/screenshot/download", HTTP_GET, [](AsyncWebServerRequest* request) {
        // RAM path
        if (request->hasParam("ram")) {
            if (!isScreenshotRamReady()) {
                request->send(503, "application/json", "{\"error\":\"RAM capture not ready\"}");
                return;
            }
            // ESPAsyncWebServer 3.x filler overload: no status code arg (defaults to 200)
            AsyncWebServerResponse* resp = request->beginResponse(
                "image/bmp", getScreenshotRamSize(),
                [](uint8_t* dst, size_t maxLen, size_t index) -> size_t {
                    const uint8_t* src = getScreenshotRamBuf();
                    size_t total = getScreenshotRamSize();
                    if (!src || index >= total) return 0;
                    size_t toCopy = min(maxLen, total - index);
                    memcpy(dst, src + index, toCopy);
                    return toCopy;
                }
            );
            resp->addHeader("Content-Disposition", "attachment; filename=\"screenshot.bmp\"");
            resp->addHeader("Cache-Control", "no-store");
            request->send(resp);
            return;
        }

        // SD path
        if (!isScreenshotReady()) {
            request->send(503, "application/json", "{\"error\":\"SD not ready\"}");
            return;
        }
        if (!request->hasParam("file")) {
            request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}");
            return;
        }
        String fullPath = "/shots/" + request->getParam("file")->value();
        if (!SD.exists(fullPath)) {
            request->send(404, "application/json", "{\"error\":\"File not found\"}");
            return;
        }
        request->send(SD, fullPath, "image/bmp", true);
    });

    // POST screenshot capture — queued for main-loop execution
    // Works with or without SD card: SD path saves to file, RAM path captures to heap buffer.
    server.on("/api/screenshot", HTTP_POST, [](AsyncWebServerRequest* request) {
        JsonDocument doc;

        if (isScreenshotReady()) {
            // SD card present: capture to file; client polls /api/screenshot/status for result
            bool queued = requestScreenshot("web");
            doc["ok"] = queued;
            doc["queued"] = queued;
            if (!queued) doc["error"] = "Queue failed";
        } else {
            // No SD card: capture to RAM buffer
            bool queued = requestScreenshotToRam();
            doc["ok"] = queued;
            doc["queued"] = queued;
            doc["ram"] = true;
            if (!queued) doc["error"] = getLastScreenshotError();
        }

        String json;
        serializeJson(doc, json);
        request->send(doc["ok"] ? 200 : 500, "application/json", json);
    });
#endif

    // ElegantOTA
    ElegantOTA.begin(&server);

    server.begin();
    DBG_INFO("[Web] Server started on port %d", WEB_SERVER_PORT);
}
