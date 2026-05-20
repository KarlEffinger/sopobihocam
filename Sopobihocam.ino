// ============================================================
// Sopobihocam – Solar Powered Bird House Camera
// ESP32-S3 CAM Firmware (Freenove Board, OV2640 NoIR)
//
// compile / upload with
// - ESP32S3 Dev Module
// - PSRAM OPI PSRAM
// - Partition Scheme: Default 8MB with spiffs
//
// Zustandsautomat:
//   BOOT → MeasureBatt
//     ├─ batt low → WiFi → NTP → SendMail (nur wenn !mail_sent) → NightSleep (bis sleep_start_h)
//     └─ batt OK → Camera Init → TryWiFi (STA) → Ready
//                                              └─ Timeout → StartAP → Ready
//   Ready: Web-UI (Port 80), MJPEG-Stream + Snapshot (Port 81)
//     ├─ Kein Client seit wait_for_client_s → DeepSleep
//     └─ Stream-Client verbunden → wach bis Client trennt → DeepSleep
//   DeepSleep: Timer-Wakeup nach sleep_interval_s (Standard: 60s)
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"

// Deklaration aus stream_server.cpp (kein Auto-Prototyp für .cpp-Dateien)
void startStreamServer();

// --- Globale Variablen ---
Config cfg;
unsigned long lastClientActivity = 0;
bool apMode = false;
uint8_t ledCurrentPct = 0;
bool rgbBlinking = false;
volatile bool streamClientConnected = false;
float batt_v_at_boot = 0.0f;  // Batteriespannung beim Boot (für Nacht-Health-Mail)

void setup() {
  Serial.begin(115200);
  delay(100);
  loggerInit();       // Filesystem-Logger so früh wie möglich starten
  applyTimezone();    // TZ-Variable setzen (geht im Deep Sleep verloren) → Log-Zeitstempel in Lokalzeit

  // --- RGB-LED: schwaches Blau = Boot läuft ---
  rgbLedWrite(LED_RGB_PIN, 0, 0, 20);

  // --- IR-LED GPIO vorab sicher auf LOW (LEDC-Init erfolgt erst nach initCamera(),
  //     damit kein Konflikt mit LEDC_CHANNEL_0/TIMER_0 der Kamera entsteht) ---
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW);

  Serial.println("\n=============================");
  Serial.println("  Sopobihocam – Boot");
  Serial.println("=============================");

  // --- Wake-Up-Grund und Reset-Ursache loggen ---
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    logLine("[BOOT] Wake-Up durch Timer\n");
  } else {
    const char* resetReason = "Unbekannt";
    switch (esp_reset_reason()) {
      case ESP_RST_POWERON:   resetReason = "Einschalten";        break;
      case ESP_RST_EXT:       resetReason = "Externer Reset";     break;
      case ESP_RST_SW:        resetReason = "Software-Reset";     break;
      case ESP_RST_PANIC:     resetReason = "Panic/Exception";    break;
      case ESP_RST_INT_WDT:   resetReason = "Interrupt-Watchdog"; break;
      case ESP_RST_TASK_WDT:  resetReason = "Task-Watchdog";      break;
      case ESP_RST_WDT:       resetReason = "Watchdog";           break;
      case ESP_RST_BROWNOUT:  resetReason = "Brownout";           break;
      default: break;
    }
    logLine("[BOOT] Reset – Grund: %s\n", resetReason);
  }

  // --- NVS laden ---
  loadConfig();

  // --- Batteriespannung messen ---
  float batt_v = measureBatteryVoltage();
  batt_v_at_boot = batt_v;
  bool usbPowered = (batt_v < BATT_NO_BATTERY_V);

  if (usbPowered) {
    logLine("[BOOT] Kein Akku erkannt, USB-Betrieb\n");
  }

  // --- Unterspannung: Mail senden (max. einmal) + langer Nachtschlaf ---
  // Egal ob mail_sent_flag true oder false: bei Unterspannung immer in Nachtschlaf,
  // damit der Akku nicht durch 60s-Kurzschlaf-Zyklen weiter entladen wird.
  if (!usbPowered && batt_v < cfg.batt_min_v) {
    if (!cfg.mail_sent_flag) {
      logLine("[BOOT] Batterie niedrig (%.2fV < %.2fV), sende Warnung\n",
              batt_v, cfg.batt_min_v);
      if (connectSTA(WIFI_STA_TIMEOUT_MS)) {
        syncNTP();   // Für secondsUntilMorning() – Fehler unkritisch
        sendWarningMail(batt_v);
        saveMailSentFlag(true);
      } else {
        logLine("[BOOT] Kein WiFi für Mailversand\n");
      }
    } else {
      logLine("[BOOT] Batterie niedrig (%.2fV), mail_sent_flag gesetzt – direkt Nachtschlaf\n",
              batt_v);
    }
    enterNightSleep();   // Langer Schlaf bis sleep_start_h statt 60s-Kurzschlaf
    return;
  }

  // Batterie OK → mail_sent_flag zurücksetzen
  if (!usbPowered && batt_v >= cfg.batt_min_v && cfg.mail_sent_flag) {
    logLine("[BOOT] Batterie OK, setze mail_sent_flag zurück\n");
    saveMailSentFlag(false);
  }

  // ================================================================
  // MODUS 0 – Täglicher Modus: einmal aufwachen, Foto + Mail senden,
  //           IMAP prüfen, dann bis zum nächsten Morgen schlafen.
  //           Kein Web-UI, kein Stream-Server, IR-LED bleibt aus.
  // ================================================================
  if (cfg.mode == 0) {
    logLine("[BOOT] Modus 0 (täglich) – Kamera init\n");

    // Kamera initialisieren (für optionalen Snapshot in Health-Mail)
    bool cameraOk = initCamera();
    if (!cameraOk) {
      logLine("[BOOT] Kamera-Init fehlgeschlagen – Mail ohne Snapshot\n");
    }

    // WiFi verbinden – bei Fehler direkt in Nachtschlaf
    if (!connectSTA(WIFI_STA_TIMEOUT_MS)) {
      logLine("[BOOT] Modus 0: kein WiFi – Nachtschlaf\n");
      if (cameraOk) esp_camera_deinit();
      enterNightSleep();
      return;
    }

    // NTP synchronisieren (für korrekte Zeitstempel und secondsUntilMorning())
    syncNTP();

    // Morgen-Health-Mail senden (einmal pro Tag, enthält mailto-Umschalt-Link)
    logLine("[BOOT] Modus 0 – sende Health-Mail\n");
    checkAndSendMorningHealthMail(batt_v_at_boot);

    // IMAP prüfen: Befehlsmail vorhanden → auf Modus 1 (aktiv) umschalten
    logLine("[BOOT] Modus 0 – prüfe IMAP auf Befehlsmail\n");
    if (checkImapForCommand()) {
      logLine("[BOOT] Modus 0 → 1: Befehlsmail empfangen, wechsle auf aktiven Modus\n");
      cfg.mode = 1;
      saveConfig();
      // Kurzer DeepSleep: beim nächsten Wake startet Modus 1
      if (cameraOk) esp_camera_deinit();
      enterDeepSleep();
      return;
    }

    // Kein Befehl → bis zum nächsten Morgen schlafen
    logLine("[BOOT] Modus 0 – kein Befehl, Nachtschlaf\n");
    if (cameraOk) esp_camera_deinit();
    enterNightSleep();
    return;
  }

  // ================================================================
  // MODUS 1 – Aktiver Modus: bisheriges Verhalten (Polling, Web-UI,
  //           Stream-Server, alle sleep_interval_s aufwachen).
  // ================================================================

  // --- Kamera initialisieren ---
  // IR-LED LEDC erst nach initCamera() konfigurieren: Kamera belegt LEDC_CHANNEL_0/TIMER_0
  // (20 MHz XCLK), danach wählt ledcAttach() automatisch einen konfliktfreien Timer.
  if (!initCamera()) {
    logLine("[BOOT] Kamera-Init fehlgeschlagen, sende Warnmail\n");
    if (connectSTA(WIFI_STA_TIMEOUT_MS)) {
      sendWarningMail(batt_v);
    }
    enterDeepSleep();
    return;
  }

  // --- IR-LED LEDC nach erfolgreichem Camera-Init konfigurieren ---
  irLedSetup();

  // --- WiFi-Verbindung ---
  if (!connectSTA(WIFI_STA_TIMEOUT_MS)) {
    startAP();
  }

  // --- NTP-Sync (nur im STA-Modus) ---
  if (!apMode) {
    syncNTP();
  }
  // Tageszeit erkannt → Morgen-Health-Mail senden (einmal pro Tag)
  if (!apMode && !isNightTime()) {
    checkAndSendMorningHealthMail(batt_v_at_boot);
  }

  // --- mDNS ---
  setupMDNS();

  // --- WebServer (Port 80) und Stream-Server (Port 81) starten ---
  setupWebServer();
  startStreamServer();

  // --- Aktivitäts-Timer initialisieren ---
  lastClientActivity = millis();

  // --- RGB-LED: schwaches Grün = Kamera + WiFi OK ---
  rgbLedWrite(LED_RGB_PIN, 0, 20, 0);
  delay(1000);

  // --- IR-LED einblenden, RGB wechselt zu schwachem Rot (Kamera aktiv) ---
  fadeLedOn();

  logLine("[BOOT] Bereit – warte %us auf Client, Schlafintervall: %us\n",
          cfg.wait_for_client_s, cfg.sleep_interval_s);
}

void loop() {
  server.handleClient();

  // RGB-Blinken im AP-Modus: hell rot blinken (500ms Takt)
  if (rgbBlinking) {
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    if (millis() - lastBlink >= 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      rgbLedWrite(LED_RGB_PIN, blinkState ? 200 : 0, 0, 0);
    }
  }

  // Sleep wenn kein Stream-Client verbunden und keine Web-UI-Aktivität
  if (!streamClientConnected &&
      millis() - lastClientActivity > (unsigned long)cfg.wait_for_client_s * 1000UL) {
    if (!apMode && isNightTime()) {
      logLine("[LOOP] Nachtfenster (%02d:00–%02d:00) – Nachtschlaf\n",
              cfg.sleep_end_h, cfg.sleep_start_h);
      enterNightSleep();
    } else {
      logLine("[LOOP] Kein Client seit %us, gehe in Sleep\n", cfg.wait_for_client_s);
      enterDeepSleep();
    }
  }
}
