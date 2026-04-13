// ============================================================
// Zeitverwaltung – NTP-Synchronisation und Nachtschlaf-Logik
//
// Timezone: Deutschland (CET/CEST, automatische Sommerzeit)
//
// Zeitrekonstruktion über zwei RTC-Variablen:
//   rtc_ntp_unix_ts:  UTC-Zeitpunkt des Boots beim letzten NTP-Sync
//                     (= NTP-Zeit minus millis()/1000 zum Sync-Zeitpunkt)
//   rtc_ntp_accum_s:  akkumulierte Sekunden seit NTP-Sync über Sleep-Zyklen
//
// Formel: getEstimatedTime() = rtc_ntp_unix_ts + rtc_ntp_accum_s + millis()/1000
// Genauigkeit: ESP32-S3 interner RC-Oszillator ca. ±5 % → bei 14h Nachtschlaf
// bis zu ~42 Minuten Drift. Daher: bei jedem Boot NTP-Sync erzwingen (syncNTP
// setzt Systemuhr auf Epoche zurück, damit getLocalTime auf echte Antwort wartet).
// ============================================================

#include <time.h>

#define TZ_GERMANY "CET-1CEST,M3.5.0,M10.5.0/3"

RTC_DATA_ATTR uint32_t rtc_ntp_unix_ts = 0;  // Boot-Zeitpunkt UTC (nach letztem NTP-Sync)
RTC_DATA_ATTR uint32_t rtc_ntp_accum_s = 0;  // akkumulierte Zeit seit NTP-Sync (Sleep + Wake)

void applyTimezone() {
  setenv("TZ", TZ_GERMANY, 1);
  tzset();
}

// NTP synchronisieren (nur aufrufen wenn WiFi verbunden)
// configTzTime() setzt POSIX-Timezone und NTP in einem Aufruf – time() liefert UTC,
// localtime_r() liefert korrekte deutsche Lokalzeit (inkl. automatischer Sommerzeit).
// rtc_ntp_unix_ts wird auf den Boot-Zeitpunkt normiert (millis()==0),
// damit die Formel rtc_ntp_unix_ts + accum + millis()/1000 immer stimmt.
//
// WICHTIG: Die ESP32-Systemuhr überlebt Deep Sleep (Hardware-RTC läuft weiter),
// driftet aber mit dem internen RC-Oszillator (~±5 %). getLocalTime() prüft nur
// ob Jahr > 2016 → würde die gedriftete RTC-Zeit sofort als "gültig" akzeptieren,
// OHNE auf eine echte NTP-Antwort zu warten. Deshalb: Systemuhr vor SNTP-Start
// auf Epoche zurücksetzen, damit getLocalTime() auf die echte Antwort wartet.
bool syncNTP() {
  // Systemuhr auf Epoche (1970) zurücksetzen → erzwingt echten NTP-Sync
  struct timeval tv_reset = { .tv_sec = 0, .tv_usec = 0 };
  settimeofday(&tv_reset, NULL);

  configTzTime(TZ_GERMANY, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    logLine("[NTP] Sync fehlgeschlagen (Timeout 10s)\n");
    return false;
  }
  time_t now;
  time(&now);  // UTC
  // Normierung: Basis = Boot-Zeitpunkt (millis=0)
  rtc_ntp_unix_ts = (uint32_t)now - millis() / 1000;
  rtc_ntp_accum_s = 0;
  char buf[32];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
  logLine("[NTP] Sync OK: %s (DE Lokalzeit)\n", buf);
  return true;
}

// Öffentlicher Wrapper für getEstimatedTime() – wird von logger.ino verwendet
time_t getLogTime() { return getEstimatedTime(); }

// Geschätzte aktuelle UTC-Zeit aus RTC + Akkumulator + laufendem millis()
static time_t getEstimatedTime() {
  if (rtc_ntp_unix_ts == 0) return 0;  // noch nie synchronisiert
  return (time_t)(rtc_ntp_unix_ts + rtc_ntp_accum_s + millis() / 1000);
}

// True wenn die aktuelle Lokalzeit im konfigurierten Nachtfenster liegt
// Nacht: Stunde >= sleep_end_h  ODER  Stunde < sleep_start_h
bool isNightTime() {
  time_t now = getEstimatedTime();
  if (now == 0) return false;  // Keine Zeitinfo → kein Nachtmodus
  applyTimezone();
  struct tm t;
  localtime_r(&now, &t);
  int h = t.tm_hour;
  return (h >= cfg.sleep_end_h || h < cfg.sleep_start_h);
}

// Sekunden bis zum nächsten sleep_start_h (Aufwach-Uhrzeit)
// Fallback: cfg.sleep_interval_s falls Zeit unbekannt
uint32_t secondsUntilMorning() {
  time_t now = getEstimatedTime();
  if (now == 0) return 8UL * 3600UL;  // Fallback 8h wenn kein NTP
  applyTimezone();
  struct tm t;
  localtime_r(&now, &t);
  int hours_until = (int)cfg.sleep_start_h - t.tm_hour;
  if (hours_until <= 0) hours_until += 24;
  int32_t secs = hours_until * 3600 - t.tm_min * 60 - t.tm_sec;
  if (secs < 60) secs = 60;  // Mindestschlaf 60 Sekunden
  return (uint32_t)secs;
}

// Aktuelle Lokalzeit als String für die Web-UI (z.B. "14:32 CEST")
void getTimeString(char *buf, size_t len) {
  time_t now = getEstimatedTime();
  if (now == 0) {
    snprintf(buf, len, "--:-- (kein NTP)");
    return;
  }
  applyTimezone();
  struct tm t;
  localtime_r(&now, &t);
  char tz_name[8] = "";
  strftime(tz_name, sizeof(tz_name), "%Z", &t);
  snprintf(buf, len, "%02d:%02d %s", t.tm_hour, t.tm_min, tz_name);
}
