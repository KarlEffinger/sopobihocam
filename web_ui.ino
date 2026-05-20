// ============================================================
// Web-UI – Konfigurations-Seite und AJAX-Endpoints (Port 80)
// ============================================================

#include <WebServer.h>
#include <Update.h>

WebServer server(80);

// --- HTML-Seite als PROGMEM ---
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Sopobihocam Konfiguration</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee;
         max-width: 600px; margin: 0 auto; padding: 16px; }
  h1 { text-align: center; color: #e94560; margin-bottom: 20px; font-size: 1.4em; }
  h2 { color: #0f3460; background: #e94560; padding: 8px 12px; margin: 16px -4px 12px;
       border-radius: 4px; font-size: 1em; }
  label { display: block; margin: 8px 0 4px; font-size: 0.9em; color: #ccc; }
  input[type=text], input[type=password], input[type=number] {
    width: 100%; padding: 8px; border: 1px solid #444; border-radius: 4px;
    background: #16213e; color: #eee; font-size: 0.95em; }
  input[type=range] {
    width: 100%; accent-color: #e94560; cursor: pointer; margin-top: 6px; }
  .row { display: flex; gap: 8px; align-items: flex-end; }
  .row > div { flex: 1; }
  button { padding: 10px 16px; border: none; border-radius: 4px; cursor: pointer;
           font-size: 0.95em; margin-top: 8px; }
  .btn-save { background: #e94560; color: #fff; width: 100%; margin-top: 20px; }
  .btn-cal { background: #0f3460; color: #fff; }
  .info { background: #16213e; border: 1px solid #333; border-radius: 4px;
          padding: 10px; margin: 8px 0; font-size: 0.9em; }
  .batt-display { font-size: 1.3em; color: #e94560; font-weight: bold; }
  #status { text-align: center; margin-top: 12px; color: #53d769; font-weight: bold; }
  .result { margin-top: 6px; font-size: 0.9em; color: #53d769; font-weight: bold; min-height: 1.2em; }
  .tabs { display:flex; gap:4px; margin-bottom:4px; flex-wrap:wrap;
          position:sticky; top:0; z-index:10; background:#1a1a2e; padding:8px 0 4px; }
  .tab-btn { background:#16213e; color:#ccc; flex:1; padding:10px 4px; font-size:0.85em; min-width:0;
             border:1px solid #333; border-radius:4px 4px 0 0; margin:0; }
  .tab-btn.active { background:#e94560; color:#fff; border-color:#e94560; }
</style>
</head>
<body>
<h1>Sopobihocam</h1>

<div class="tabs">
  <button class="tab-btn active" onclick="showTab(0)">&#127909; Kamera</button>
  <button class="tab-btn" onclick="showTab(1)">&#9201; Zeitplan</button>
  <button class="tab-btn" onclick="showTab(2)">&#9993; Mail</button>
  <button class="tab-btn" onclick="showTab(3)">&#9881; System</button>
</div>

<!-- Tab 0: Kamera -->
<div class="tab-panel">
<h2>Live-Vorschau</h2>
<div style="text-align:center;margin:8px 0">
  <img id="preview" style="max-width:100%;border-radius:4px;background:#111;min-height:120px" alt="Kamera">
</div>
<h2>Beleuchtung</h2>
<div class="row" style="align-items:center">
  <div style="flex:3">
    <label>IR-LED Helligkeit (0 = aus)</label>
    <input type="range" id="led_brightness" min="0" max="100" value="%LED_BRIGHT%" oninput="setLed(this.value)">
  </div>
  <div style="flex:1;text-align:center;font-size:1.2em;color:#e94560;font-weight:bold">
    <span id="led_pct">%LED_BRIGHT%%</span>
  </div>
</div>
<h2>Stream-URL</h2>
<div class="info">MJPEG-Stream (Port 81): <a id="stream_url" href="#" target="_blank">--</a></div>
</div>

<!-- Tab 1: Zeitplan -->
<div class="tab-panel" style="display:none">
<h2>Betriebsmodus</h2>
<div class="info">Modus: <b id="mode_label">%MODE_LABEL%</b></div>
<div id="setmode_section" style="display:%SETMODE_DISPLAY%">
  <button class="btn-cal" onclick="setDailyMode()" style="margin-top:8px">Zur&uuml;ck auf t&auml;glich</button>
  <div class="result" id="setmode_result"></div>
</div>
<h2>Streaming</h2>
<label>Wartezeit auf ersten Client (5&ndash;120 Sekunden)</label>
<input type="number" id="wait_for_client" value="%WAIT_CLIENT%" min="5" max="120">
<label>Schlafintervall zwischen Checks (10&ndash;3600 Sekunden)</label>
<input type="number" id="sleep_interval" value="%SLEEP_IVL%" min="10" max="3600">
<h2>Nachtruhe</h2>
<div class="info">Aktuelle Uhrzeit: <span id="time_display">--:--</span></div>
<div class="row">
  <div>
    <label>Aktiv ab (Uhr, 0&ndash;23)</label>
    <input type="number" id="sleep_start_h" value="%SLEEP_START_H%" min="0" max="23">
  </div>
  <div>
    <label>Aktiv bis (Uhr, 0&ndash;23)</label>
    <input type="number" id="sleep_end_h" value="%SLEEP_END_H%" min="0" max="23">
  </div>
</div>
<h2>Batterie</h2>
<div class="info">
  Aktuelle Spannung: <span class="batt-display" id="batt_v">--</span> V
</div>
<label>Mindestspannung f&uuml;r Warnung (V)</label>
<input type="number" id="batt_min_v" value="%BATT_MIN%" step="0.1" min="2.5" max="4.2">
<div id="batt_warn_section" style="display:%BATT_WARN_DISPLAY%">
  <div class="info" style="color:#e94560;margin-top:8px">&#9888; Batteriewarnung wurde gesendet</div>
  <button class="btn-cal" onclick="resetMailFlag()">Batteriewarnung zur&uuml;cksetzen</button>
  <div class="result" id="mail_flag_result"></div>
</div>
</div>

<!-- Tab 2: Mail -->
<div class="tab-panel" style="display:none">
<h2>Mail-Warnung</h2>
<label>SMTP-Host</label>
<input type="text" id="mail_host" value="%MAIL_HOST%">
<div class="row">
  <div>
    <label>Port</label>
    <input type="number" id="mail_port" value="%MAIL_PORT%" min="1" max="65535">
  </div>
</div>
<label>Benutzer</label>
<input type="text" id="mail_user" value="%MAIL_USER%">
<label>Passwort</label>
<input type="password" id="mail_pass" value="%MAIL_PASS%">
<label>Empf&auml;nger</label>
<input type="text" id="mail_to" value="%MAIL_TO%">
<label style="display:flex;align-items:center;gap:8px;margin:10px 0;cursor:pointer">
  <input type="checkbox" id="mail_snapshot" %MAIL_SNAPSHOT%> Schnappschuss mitsenden
</label>
<label style="display:flex;align-items:center;gap:8px;margin:10px 0;cursor:pointer">
  <input type="checkbox" id="mail_log" %MAIL_LOG%> Protokoll mitsenden
</label>
<h2>IMAP-Empfang (Befehlsmail)</h2>
<label>IMAP-Host</label>
<input type="text" id="imap_host" value="%IMAP_HOST%">
<label>IMAP-Port</label>
<input type="number" id="imap_port" value="%IMAP_PORT%" min="1" max="65535">
<label>Betreff-Befehlsstring (zum Umschalten auf aktiven Modus)</label>
<input type="text" id="imap_cmd" value="%IMAP_CMD%">
<button class="btn-cal" onclick="testMail()">Verbindung testen</button>
<div class="result" id="mail_test_result"></div>
</div>

<!-- Tab 3: System -->
<div class="tab-panel" style="display:none">
<div class="info" style="text-align:center;color:#aaa;font-size:0.85em">Firmware: <b id="fw_ver">%FW_VERSION%</b></div>
<h2>WLAN</h2>
<label>SSID</label>
<input type="text" id="wifi_ssid" value="%WIFI_SSID%">
<label>Passwort</label>
<input type="password" id="wifi_pass" value="%WIFI_PASS%">
<h2>Diagnose</h2>
<div class="info">Log-Datei: <span id="log_size">--</span> KB / %LOG_MAX_KB% KB</div>
<div class="row" style="margin-top:8px">
  <button class="btn-cal" onclick="window.location.href='/log'" style="flex:1">&#128203; Herunterladen</button>
  <button class="btn-cal" onclick="clearLog()" style="flex:1;background:#555">&#128465; Log l&ouml;schen</button>
</div>
<div class="result" id="log_result"></div>
<h2>Firmware-Update (OTA)</h2>
<div class="info">Firmware-Datei (.bin) direkt &uuml;ber den Browser hochladen.<br>
Die Kamera startet nach erfolgreichem Update automatisch neu.</div>
<input type="file" id="ota_file" accept=".bin" style="width:100%;padding:6px 0;color:#eee;margin:8px 0">
<button class="btn-cal" onclick="startOTA()">&#128640; Firmware hochladen</button>
<div id="ota_progress" style="display:none;margin-top:8px">
  <progress id="ota_bar" value="0" max="100" style="width:100%;accent-color:#e94560;height:8px"></progress>
  <div id="ota_status" style="text-align:center;margin-top:4px;font-size:0.9em;color:#ccc"></div>
</div>
</div>

<button class="btn-save" onclick="saveConfig()">Speichern</button>
<div id="status"></div>

<script>
function showTab(n) {
  document.querySelectorAll('.tab-panel').forEach(function(p, i) {
    p.style.display = i === n ? 'block' : 'none';
  });
  document.querySelectorAll('.tab-btn').forEach(function(b, i) {
    b.classList.toggle('active', i === n);
  });
}

function fetchBattery() {
  fetch('/battery').then(r => r.json()).then(d => {
    document.getElementById('batt_v').textContent = d.voltage.toFixed(2);
  }).catch(() => {});
}

function saveConfig() {
  var params = new URLSearchParams();
  params.append('wifi_ssid',      document.getElementById('wifi_ssid').value);
  params.append('wifi_pass',      document.getElementById('wifi_pass').value);
  params.append('wait_for_client', document.getElementById('wait_for_client').value);
  params.append('sleep_interval', document.getElementById('sleep_interval').value);
  params.append('batt_min_v',     document.getElementById('batt_min_v').value);
  params.append('mail_host',      document.getElementById('mail_host').value);
  params.append('mail_port',      document.getElementById('mail_port').value);
  params.append('mail_user',      document.getElementById('mail_user').value);
  params.append('mail_pass',      document.getElementById('mail_pass').value);
  params.append('mail_to',        document.getElementById('mail_to').value);
  params.append('sleep_start_h',  document.getElementById('sleep_start_h').value);
  params.append('sleep_end_h',    document.getElementById('sleep_end_h').value);
  params.append('mail_snapshot',  document.getElementById('mail_snapshot').checked ? '1' : '0');
  params.append('mail_log',       document.getElementById('mail_log').checked ? '1' : '0');
  params.append('imap_host',      document.getElementById('imap_host').value);
  params.append('imap_port',      document.getElementById('imap_port').value);
  params.append('imap_cmd',       document.getElementById('imap_cmd').value);

  fetch('/save', { method: 'POST', body: params }).then(r => r.json()).then(d => {
    var el = document.getElementById('status');
    el.textContent = d.ok ? 'Gespeichert!' : 'Fehler beim Speichern';
    setTimeout(() => { el.textContent = ''; }, 3000);
  }).catch(() => {
    var el = document.getElementById('status');
    el.textContent = 'Verbindungsfehler';
    setTimeout(() => { el.textContent = ''; }, 3000);
  });
}

function setDailyMode() {
  var el = document.getElementById('setmode_result');
  el.textContent = '...';
  fetch('/setmode?mode=0').then(r => r.json()).then(d => {
    if (d.ok) {
      document.getElementById('mode_label').textContent = 'Täglich (1× pro Tag)';
      document.getElementById('setmode_section').style.display = 'none';
      el.textContent = 'Modus gespeichert.';
      setTimeout(() => { el.textContent = ''; }, 3000);
    } else {
      el.textContent = 'Fehler';
      setTimeout(() => { el.textContent = ''; }, 3000);
    }
  }).catch(() => { el.textContent = 'Verbindungsfehler'; });
}

function resetMailFlag() {
  var el = document.getElementById('mail_flag_result');
  el.textContent = '...';
  fetch('/resetmailflag').then(r => r.json()).then(d => {
    if (d.ok) {
      document.getElementById('batt_warn_section').style.display = 'none';
    } else {
      el.textContent = 'Fehler';
      setTimeout(() => { el.textContent = ''; }, 3000);
    }
  }).catch(() => { el.textContent = 'Verbindungsfehler'; });
}

function testMail() {
  var el = document.getElementById('mail_test_result');
  el.textContent = 'Teste Verbindung...';
  var params = new URLSearchParams();
  params.append('mail_host', document.getElementById('mail_host').value);
  params.append('mail_port', document.getElementById('mail_port').value);
  params.append('mail_user', document.getElementById('mail_user').value);
  params.append('mail_pass', document.getElementById('mail_pass').value);
  fetch('/testmail', { method: 'POST', body: params })
    .then(r => r.json())
    .then(d => {
      el.textContent = d.ok ? 'Verbindung OK' : 'Fehler: ' + d.error;
      if (d.ok) setTimeout(() => { el.textContent = ''; }, 3000);
    })
    .catch(() => { el.textContent = 'Verbindungsfehler'; });
}

function setLed(val) {
  document.getElementById('led_pct').textContent = val + '%';
  fetch('/led?brightness=' + val).catch(() => {});
}

// Aktuelle Uhrzeit anzeigen (NTP-Status)
function fetchTime() {
  fetch('/time').then(r => r.json()).then(d => {
    document.getElementById('time_display').textContent = d.time;
  }).catch(() => {});
}
fetchTime();
setInterval(fetchTime, 60000);

// Heartbeat: hält den Aktivitäts-Timer zurück solange die Seite offen ist
setInterval(() => fetch('/ping').catch(() => {}), %HEARTBEAT_MS%);

// Stream-URL dynamisch setzen
(function() {
  var host = window.location.hostname;
  var sUrl = 'http://' + host + ':81/stream';
  var el = document.getElementById('stream_url');
  el.href = sUrl; el.textContent = sUrl;
})();

// Live-Vorschau: JPEG-Polling (Port 81)
(function() {
  var preview = document.getElementById('preview');
  function nextFrame() {
    var img = new Image();
    img.onload  = function() { preview.src = this.src; setTimeout(nextFrame, 100); };
    img.onerror = function() { setTimeout(nextFrame, 1000); };
    img.src = 'http://' + window.location.hostname + ':81/jpg?' + Date.now();
  }
  nextFrame();
})();

// Batteriespannung alle 30 Sekunden aktualisieren
fetchBattery();
setInterval(fetchBattery, 30000);

// OTA-Firmware-Update
function startOTA() {
  var fileInput = document.getElementById('ota_file');
  if (!fileInput.files.length) { alert('Bitte eine .bin-Datei auswählen.'); return; }
  var formData = new FormData();
  formData.append('update', fileInput.files[0]);
  var bar    = document.getElementById('ota_bar');
  var status = document.getElementById('ota_status');
  document.getElementById('ota_progress').style.display = 'block';
  status.textContent = 'Übertrage...';
  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      var pct = Math.round(e.loaded / e.total * 100);
      bar.value = pct;
      status.textContent = pct + '% – ' + Math.round(e.loaded/1024) + ' / ' + Math.round(e.total/1024) + ' KB';
    }
  };
  xhr.onload = function() {
    try {
      var d = JSON.parse(xhr.responseText);
      if (d.ok) {
        bar.value = 100;
        var secs = 12;
        function countdown() {
          status.textContent = 'Erfolgreich! Seite lädt in ' + secs + 's neu...';
          if (secs-- > 0) setTimeout(countdown, 1000); else window.location.reload();
        }
        countdown();
      }
      else { status.textContent = 'Fehler: ' + (d.error || 'unbekannt'); }
    } catch(e) { status.textContent = 'Antwort-Fehler'; }
  };
  xhr.onerror = function() { status.textContent = 'Verbindungsfehler beim Upload'; };
  xhr.send(formData);
}

// Diagnose-Log
function clearLog() {
  if (!confirm('Log wirklich löschen?')) return;
  var el = document.getElementById('log_result');
  fetch('/clearlog').then(function(r){ return r.json(); }).then(function(d){
    el.textContent = d.ok ? 'Log gelöscht.' : 'Fehler';
    document.getElementById('log_size').textContent = '0.0';
    setTimeout(function(){ el.textContent = ''; }, 3000);
  }).catch(function(){ el.textContent = 'Verbindungsfehler'; });
}
fetch('/logsize').then(function(r){ return r.json(); }).then(function(d){
  document.getElementById('log_size').textContent = (d.size / 1024).toFixed(1);
}).catch(function(){});
</script>
</body>
</html>
)rawliteral";

// Platzhalter in HTML ersetzen
String buildConfigPage() {
  String page = String(CONFIG_PAGE);
  page.replace("%WIFI_SSID%",   cfg.wifi_ssid);
  page.replace("%WIFI_PASS%",   cfg.wifi_pass);
  page.replace("%WAIT_CLIENT%", String(cfg.wait_for_client_s));
  page.replace("%SLEEP_IVL%",   String(cfg.sleep_interval_s));
  page.replace("%BATT_MIN%",         String(cfg.batt_min_v, 1));
  page.replace("%BATT_WARN_DISPLAY%", cfg.mail_sent_flag ? "block" : "none");
  page.replace("%MAIL_HOST%",   cfg.mail_host);
  page.replace("%MAIL_PORT%",   String(cfg.mail_port));
  page.replace("%MAIL_USER%",   cfg.mail_user);
  page.replace("%MAIL_PASS%",   cfg.mail_pass);
  page.replace("%MAIL_TO%",     cfg.mail_to);
  page.replace("%LED_BRIGHT%",    String(cfg.led_brightness));
  page.replace("%SLEEP_START_H%", String(cfg.sleep_start_h));
  page.replace("%SLEEP_END_H%",   String(cfg.sleep_end_h));
  page.replace("%MAIL_SNAPSHOT%", cfg.mail_snapshot ? "checked" : "");
  page.replace("%MAIL_LOG%",      cfg.mail_log      ? "checked" : "");
  page.replace("%IMAP_HOST%",     cfg.imap_host);
  page.replace("%IMAP_PORT%",     String(cfg.imap_port));
  page.replace("%IMAP_CMD%",      cfg.imap_cmd);
  page.replace("%MODE_LABEL%",    cfg.mode == 0 ? "T&auml;glich (1&times; pro Tag)"
                                                : "Aktiv (alle " + String(cfg.sleep_interval_s) + "s)");
  page.replace("%SETMODE_DISPLAY%", cfg.mode == 1 ? "block" : "none");
  page.replace("%HEARTBEAT_MS%",  String(cfg.wait_for_client_s * 1000UL / 3));
  page.replace("%FW_VERSION%",    String(FIRMWARE_VERSION));
  page.replace("%LOG_MAX_KB%",    String(LOG_MAX_SIZE / 1024));
  return page;
}

// --- Handler ---

void handleRoot() {
  lastClientActivity = millis();
  server.send(200, "text/html", buildConfigPage());
}

void handleSave() {
  lastClientActivity = millis();

  cfg.wifi_ssid          = server.arg("wifi_ssid");
  cfg.wifi_pass          = server.arg("wifi_pass");
  cfg.wait_for_client_s  = (uint16_t)server.arg("wait_for_client").toInt();
  cfg.sleep_interval_s   = (uint16_t)server.arg("sleep_interval").toInt();
  cfg.batt_min_v         = server.arg("batt_min_v").toFloat();
  cfg.mail_host          = server.arg("mail_host");
  cfg.mail_port          = (uint16_t)server.arg("mail_port").toInt();
  cfg.mail_user          = server.arg("mail_user");
  cfg.mail_pass          = server.arg("mail_pass");
  cfg.mail_to            = server.arg("mail_to");
  cfg.sleep_start_h      = (uint8_t)constrain(server.arg("sleep_start_h").toInt(), 0, 23);
  cfg.sleep_end_h        = (uint8_t)constrain(server.arg("sleep_end_h").toInt(),   0, 23);
  cfg.mail_snapshot      = server.arg("mail_snapshot") == "1";
  cfg.mail_log           = server.arg("mail_log")      == "1";
  cfg.imap_host          = server.arg("imap_host");
  cfg.imap_port          = (uint16_t)server.arg("imap_port").toInt();
  cfg.imap_cmd           = server.arg("imap_cmd");
  if (cfg.imap_port == 0)          cfg.imap_port = DEFAULT_IMAP_PORT;
  if (cfg.imap_cmd.length() == 0) cfg.imap_cmd  = DEFAULT_IMAP_CMD;
  if (cfg.sleep_start_h >= cfg.sleep_end_h) {
    cfg.sleep_start_h = DEFAULT_SLEEP_START_H;
    cfg.sleep_end_h   = DEFAULT_SLEEP_END_H;
  }

  // Werte begrenzen
  if (cfg.wait_for_client_s <    5) cfg.wait_for_client_s =    5;
  if (cfg.wait_for_client_s >  120) cfg.wait_for_client_s =  120;
  if (cfg.sleep_interval_s  <   10) cfg.sleep_interval_s  =   10;
  if (cfg.sleep_interval_s  > 3600) cfg.sleep_interval_s  = 3600;

  saveConfig();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handlePing() {
  lastClientActivity = millis();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleResetMailFlag() {
  lastClientActivity = millis();
  saveMailSentFlag(false);
  logLine("[MAIL] mail_sent_flag manuell zurückgesetzt\n");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleTestMail() {
  lastClientActivity = millis();
  String host = server.arg("mail_host");
  int    port = server.arg("mail_port").toInt();
  String user = server.arg("mail_user");
  String pass = server.arg("mail_pass");
  String errorMsg;
  if (testMailConnection(host, port, user, pass, errorMsg)) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(200, "application/json",
                "{\"ok\":false,\"error\":\"" + errorMsg + "\"}");
  }
}

void handleTime() {
  char buf[32];
  getTimeString(buf, sizeof(buf));
  server.send(200, "application/json",
              String("{\"time\":\"") + buf + "\"}");
}

void handleBattery() {
  lastClientActivity = millis();
  float v = measureBatteryVoltage();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"voltage\":" + String(v, 3) + "}");
}

// --- LED-Steuerung ---
void setLedWarmWhite(uint8_t pct) {
  ledCurrentPct = pct;
  irLedSetDuty((uint8_t)(pct * 255UL / 100));
  if (pct > 0) {
    rgbLedWrite(LED_RGB_PIN, 20, 0, 0);  // schwaches Rot = Kamera aktiv
  } else {
    rgbLedWrite(LED_RGB_PIN, 0, 0, 0);
  }
}

void fadeLed(uint8_t fromPct, uint8_t toPct) {
  if (fromPct == toPct) return;
  int step = (toPct > fromPct) ? 1 : -1;
  int p = (int)fromPct;
  while (p != (int)toPct) {
    setLedWarmWhite((uint8_t)p);
    delay(40);
    p += step;
  }
  setLedWarmWhite(toPct);
}

void fadeLedOn()  { fadeLed(0, cfg.led_brightness); }
void fadeLedOff() { fadeLed(ledCurrentPct, 0); }

void handleLed() {
  lastClientActivity = millis();
  uint8_t pct = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 100);
  saveLedBrightness(pct);
  setLedWarmWhite(pct);
  server.send(200, "application/json",
              "{\"ok\":true,\"brightness\":" + String(pct) + "}");
}

// --- Modus umschalten ---
void handleSetMode() {
  lastClientActivity = millis();
  int m = server.arg("mode").toInt();
  if (m < 0 || m > 1) {
    server.send(200, "application/json", "{\"ok\":false,\"error\":\"Ungültiger Modus\"}");
    return;
  }
  cfg.mode = (uint8_t)m;
  saveConfig();
  logLine("[WEB] Modus auf %d gesetzt\n", cfg.mode);
  server.send(200, "application/json", "{\"ok\":true}");
}

// --- OTA-Update ---

void handleOTAUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    logLine("[OTA] Update gestartet: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      logLine("[OTA] begin() fehlgeschlagen: %s\n", Update.errorString());
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    lastClientActivity = millis();  // Kamera während Upload wach halten
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      logLine("[OTA] write() fehlgeschlagen: %s\n", Update.errorString());
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      logLine("[OTA] Update erfolgreich: %u Bytes\n", upload.totalSize);
    } else {
      logLine("[OTA] end() fehlgeschlagen: %s\n", Update.errorString());
    }
  }
}

void handleOTAResult() {
  if (Update.hasError()) {
    String err = Update.errorString();
    server.send(200, "application/json",
                "{\"ok\":false,\"error\":\"" + err + "\"}");
  } else {
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
  }
}

void setupWebServer() {
  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/ping",          HTTP_GET,  handlePing);
  server.on("/led",           HTTP_GET,  handleLed);
  server.on("/save",          HTTP_POST, handleSave);
  server.on("/battery",       HTTP_GET,  handleBattery);
  server.on("/resetmailflag", HTTP_GET,  handleResetMailFlag);
  server.on("/testmail",      HTTP_POST, handleTestMail);
  server.on("/time",          HTTP_GET,  handleTime);
  server.on("/setmode",       HTTP_GET,  handleSetMode);
  server.on("/update",        HTTP_POST, handleOTAResult, handleOTAUpload);
  server.on("/log",           HTTP_GET,  handleLogDownload);
  server.on("/clearlog",      HTTP_GET,  handleLogClear);
  server.on("/logsize",       HTTP_GET,  handleLogSize);
  server.begin();
  logLine("[WEB] Server gestartet auf Port 80\n");
}
