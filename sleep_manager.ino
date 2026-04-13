// ============================================================
// Sleep-Manager – Timer-basierter Deep Sleep
// ============================================================

// Hilfsfunktion: Vergleich für qsort (wird auch in battery.ino verwendet)
int cmpUint32(const void *a, const void *b) {
  uint32_t va = *(const uint32_t *)a;
  uint32_t vb = *(const uint32_t *)b;
  if (va < vb) return -1;
  if (va > vb) return 1;
  return 0;
}

// Hardware herunterfahren und Deep Sleep für sleep_s Sekunden einleiten.
// Aktualisiert rtc_ntp_accum_s (Sleep-Dauer + bisherige Wake-Zeit).
static void shutdownAndSleep(uint32_t sleep_s) {
  rgbBlinking = false;
  fadeLedOff();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_camera_deinit();
  // XCLK stoppen, um HF-Störungen im Schlaf zu vermeiden
  ledcDetach(LEDC_CHANNEL_0);
  pinMode(CAM_PIN_XCLK, INPUT);
  // IR-LED LEDC freigeben, Pin definiert LOW setzen
  ledcDetach(IR_LED_PIN);
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW);

  // NTP-Zeitakkumulator: Sleep-Dauer + aktuelle Wake-Laufzeit erfassen
  rtc_ntp_accum_s += sleep_s + (uint32_t)(millis() / 1000);

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup((uint64_t)sleep_s * 1000000ULL);
  Serial.flush();
  esp_deep_sleep_start();
}

// Normaler Schlaf (cfg.sleep_interval_s)
void enterDeepSleep() {
  logLine("[SLEEP] Deep Sleep für %us\n", cfg.sleep_interval_s);
  shutdownAndSleep(cfg.sleep_interval_s);
}

// Nachtschlaf bis cfg.sleep_start_h – Health-Flag zurücksetzen für nächste Morgen-Mail
void enterNightSleep() {
  uint32_t sleep_s = secondsUntilMorning();
  uint32_t h       = sleep_s / 3600;
  uint32_t m       = (sleep_s % 3600) / 60;
  logLine("[SLEEP] Nachtschlaf %uh%02um bis %02d:00 Uhr\n",
          h, m, cfg.sleep_start_h);
  prepareForNight();
  shutdownAndSleep(sleep_s);
}
