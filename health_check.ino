// ============================================================
// Health-Check – tägliche Status-Mail beim ersten Morgen-Aufwachen
//
// Einmal pro Tag: beim ersten Aufwachen nach sleep_start_h (Tagesbeginn).
// Erfordert aktive WiFi-Verbindung (STA-Modus).
//
// rtc_conn_count:            Stream-Verbindungen seit letzter Health-Mail
// rtc_health_mail_sent_today: verhindert Doppel-Versand pro Tag
//
// Beide Variablen überleben Deep Sleep, aber NICHT Power-Off.
//
// Ablauf:
//   setup() → !isNightTime() → checkAndSendMorningHealthMail()
//   enterNightSleep()        → prepareForNight() (Flag zurücksetzen)
// ============================================================

RTC_DATA_ATTR uint32_t rtc_conn_count              = 0;
RTC_DATA_ATTR bool     rtc_health_mail_sent_today   = false;

// Nacht-Vorbereitung: Flag zurücksetzen, damit die nächste Morgen-Mail gesendet wird.
// rtc_conn_count bleibt erhalten – wird erst nach erfolgreichem Mailversand zurückgesetzt.
// Aufrufen in enterNightSleep() vor shutdownAndSleep().
void prepareForNight() {
  logLine("[HEALTH] Nacht-Vorbereitung – %u Verbindungen heute\n", rtc_conn_count);
  rtc_health_mail_sent_today = false;
}

// Morgen-Health-Mail senden (nur einmal pro Tag, beim ersten Wake nach sleep_start_h).
// Aufrufen in setup() wenn Tageszeit erkannt (nach NTP-Sync).
void checkAndSendMorningHealthMail(float batt_v) {
  logLine("[HEALTH] Morgen-Wake – Verbindungen: %u, bereits gesendet: %s\n",
                rtc_conn_count, rtc_health_mail_sent_today ? "ja" : "nein");

  if (rtc_health_mail_sent_today) return;

  if (WiFi.status() != WL_CONNECTED) {
    logLine("[HEALTH] Kein WiFi – Health-Mail übersprungen, Retry nächster Wake");
    return;  // Flag bleibt false → nächster 60s-Wake versucht es erneut
  }

  if (cfg.mail_host.length() == 0 || cfg.mail_to.length() == 0) {
    logLine("[HEALTH] Keine Mail-Konfiguration – übersprungen");
    rtc_health_mail_sent_today = true;
    return;
  }

  logLine("[HEALTH] Sende Health-Mail (Snapshot: %s, Protokoll: %s)...\n",
          cfg.mail_snapshot ? "ja" : "nein", cfg.mail_log ? "ja" : "nein");

  // JPEG-Snapshot (nur wenn konfiguriert)
  camera_fb_t *fb = cfg.mail_snapshot ? esp_camera_fb_get() : nullptr;
  if (cfg.mail_snapshot && !fb) logLine("[HEALTH] Frame-Fehler – Snapshot fehlt\n");

  // Protokoll-Buffer (nur wenn konfiguriert)
  uint8_t *log_buf = nullptr;
  size_t   log_len = 0;
  if (cfg.mail_log) {
    log_len = readLogBuffer(&log_buf);
    if (log_len == 0) logLine("[HEALTH] Protokoll-Lesen fehlgeschlagen – ohne Log-Anhang\n");
  }

  bool ok = sendHealthMail(batt_v, rtc_conn_count,
                           fb  ? fb->buf  : nullptr, fb  ? fb->len  : 0,
                           log_buf, log_len);

  if (fb)      esp_camera_fb_return(fb);
  if (log_buf) free(log_buf);

  if (ok) {
    logLine("[HEALTH] Morgen-Health-Mail gesendet");
    rtc_health_mail_sent_today = true;
    rtc_conn_count = 0;  // Zähler zurücksetzen für neuen Tag
  } else {
    logLine("[HEALTH] Morgen-Health-Mail fehlgeschlagen – Retry nächster Wake");
    // Flag bleibt false → nächster 60s-Wake versucht es erneut
  }
}
