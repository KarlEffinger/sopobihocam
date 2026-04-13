// ============================================================
// Logger – Filesystem-Ringspeicher für Diagnose-Ausgaben
//
// logLine() schreibt sowohl auf Serial als auch mit Zeitstempel
// in /log.txt auf LittleFS (max. LOG_MAX_SIZE Bytes).
// Bei Überschreitung wird die ältere Hälfte verworfen.
//
// Zeitstempel-Format: [T+Xs|HH:MM:SS]
//   T+Xs    = Sekunden seit diesem Boot
//   HH:MM:SS = Wanduhrzeit (sobald NTP verfügbar, sonst --:--:--)
//
// HTTP-Endpunkte (registriert in web_ui.ino):
//   GET /log      → Download als sopobihocam_log.txt
//   GET /clearlog → Datei löschen
//   GET /logsize  → {"size": N}
// ============================================================

#include <LittleFS.h>

#define LOG_FILE "/log.txt"

static bool    _logReady    = false;
static uint8_t _writeCount  = 0;   // Rotation nur alle N Schreibvorgänge prüfen

// ---- Initialisierung ----------------------------------------

void loggerInit() {
  if (!LittleFS.begin(false)) {
    Serial.println("[LOG] Formatiere LittleFS...");
    if (!LittleFS.begin(true)) {
      Serial.println("[LOG] LittleFS-Fehler – Logging nur auf Serial");
      return;
    }
    Serial.println("[LOG] LittleFS formatiert und gemountet");
  }
  _logReady = true;
  logRotateIfNeeded();   // Bei jedem Boot prüfen – unabhängig vom Write-Zähler
  Serial.println("[LOG] Logger bereit");
}

// ---- Ringspeicher-Rotation ----------------------------------
// Ältere Hälfte der Datei verwerfen, neuere Hälfte behalten.
// Schleife: wiederholt rotieren bis Datei unter LOG_MAX_SIZE.

static void logRotateIfNeeded() {
  for (;;) {
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) return;
  size_t size = f.size();
  f.close();
  if (size <= LOG_MAX_SIZE) return;

  Serial.printf("[LOG] Rotation: %u Bytes > %u Bytes Limit\n", size, LOG_MAX_SIZE);

  f = LittleFS.open(LOG_FILE, "r");
  if (!f) return;
  f.seek(size / 2);
  // Bis zum nächsten Zeilenende vorspulen → erster beibehaltener Eintrag ist immer vollständig
  while (f.available() && f.read() != '\n') {}
  size_t keepFrom = f.position();
  size_t keepLen  = size - keepFrom;

  char* buf = (char*)malloc(keepLen + 1);
  if (!buf) {
    f.close();
    LittleFS.remove(LOG_FILE);   // Fallback: Datei löschen
    Serial.println("[LOG] Rotation: malloc fehlgeschlagen, Datei gelöscht");
    return;
  }
  size_t got = f.read((uint8_t*)buf, keepLen);
  f.close();

  File out = LittleFS.open(LOG_FILE, "w");
  if (out) {
    out.print("[...ältere Einträge gekürzt...]\n");
    out.write((uint8_t*)buf, got);
    out.close();
  }
  free(buf);
  Serial.println("[LOG] Rotation abgeschlossen");
  } // for(;;)
}

// ---- Zentrale Log-Funktion ----------------------------------

void logLine(const char* format, ...) {
  char msg[256];
  va_list args;
  va_start(args, format);
  vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  // Immer auf Serial ausgeben
  Serial.print(msg);
  size_t msgLen = strlen(msg);
  if (msgLen == 0 || msg[msgLen - 1] != '\n') Serial.println();

  if (!_logReady) return;

  // Zeitstempel aufbauen
  uint32_t secs = millis() / 1000;
  char ts[20] = "--.--.-- --:--:--";
  time_t now = getLogTime();    // Wrapper in time_manager.ino
  if (now > 0) {
    struct tm t;
    localtime_r(&now, &t);
    snprintf(ts, sizeof(ts), "%02d.%02d.%02d %02d:%02d:%02d",
             t.tm_mday, t.tm_mon + 1, t.tm_year % 100,
             t.tm_hour, t.tm_min, t.tm_sec);
  }
  char prefix[40];
  snprintf(prefix, sizeof(prefix), "[T+%us|%s] ", secs, ts);

  // In Datei schreiben (append)
  File f = LittleFS.open(LOG_FILE, "a");
  if (!f) return;
  f.print(prefix);
  f.print(msg);
  if (msgLen == 0 || msg[msgLen - 1] != '\n') f.println();
  f.close();

  // Rotation alle 25 Schreibvorgänge prüfen
  if (++_writeCount >= 25) {
    _writeCount = 0;
    logRotateIfNeeded();
  }
}

// ---- Log-Buffer für Mail-Anhang -----------------------------

// Liest /log.txt in einen malloc'd Buffer. Rückgabe: Anzahl gelesener Bytes (0 = Fehler).
// Aufrufer ist für free(buf) verantwortlich.
size_t readLogBuffer(uint8_t **out_buf) {
  *out_buf = nullptr;
  if (!_logReady || !LittleFS.exists(LOG_FILE)) return 0;
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) return 0;
  size_t len = f.size();
  if (len == 0) { f.close(); return 0; }
  uint8_t *buf = (uint8_t*)malloc(len);
  if (!buf) { f.close(); Serial.println("[LOG] readLogBuffer: malloc fehlgeschlagen"); return 0; }
  size_t got = f.read(buf, len);
  f.close();
  *out_buf = buf;
  return got;
}

// ---- HTTP-Handler -------------------------------------------

void handleLogDownload() {
  if (!_logReady || !LittleFS.exists(LOG_FILE)) {
    server.send(404, "text/plain", "Kein Log vorhanden");
    return;
  }
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) {
    server.send(500, "text/plain", "Lesefehler");
    return;
  }
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"sopobihocam_log.txt\"");
  server.streamFile(f, "text/plain");
  f.close();
}

void handleLogClear() {
  if (_logReady) LittleFS.remove(LOG_FILE);
  server.send(200, "application/json", "{\"ok\":true}");
  logLine("[LOG] Log manuell gelöscht\n");
}

void handleLogSize() {
  size_t s = 0;
  if (_logReady && LittleFS.exists(LOG_FILE)) {
    File f = LittleFS.open(LOG_FILE, "r");
    if (f) { s = f.size(); f.close(); }
  }
  server.send(200, "application/json", "{\"size\":" + String(s) + "}");
}
