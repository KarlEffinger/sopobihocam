# Sopobihocam

**So**lar **Po**wered **Bi**rd **Ho**use **Cam** — Eine solar- und akkubetriebene Nistkastenüberwachung auf Basis des ESP32-S3. Die Kamera wacht auf Anfrage per Browser auf, überträgt Live-Video per MJPEG-Stream, schläft nachts automatisch durch und sendet morgens eine Status-E-Mail mit Akkuspannung, Verbindungsanzahl und optional Schnappschuss sowie Diagnoseprotokoll. Konfiguration über eine tab-basierte Weboberfläche; Firmware-Updates kabellos per OTA.

---

## Funktionsübersicht

- **MJPEG-Live-Stream** per Browser, ohne App oder Sondersoftware
- **On-Demand-Betrieb**: wacht alle `sleep_interval_s` Sekunden (Standard: 60 s) auf, wartet `wait_for_client_s` Sekunden (Standard: 15 s) auf einen Browser-Client, schläft sonst sofort wieder ein
- **Nachtmodus**: automatischer Langschlaf von `sleep_end_h` bis `sleep_start_h` (Standard: 22:00–08:00 Uhr), NTP-basiert mit automatischer Sommerzeit (CET/CEST)
- **IR-LED** (GPIO14, PWM-gesteuert) für Nachtaufnahmen, Helligkeit per Schieberegler einstellbar
- **Tab-basierte Konfigurationswebseite** (Port 80) mit 4 Reitern:
  - **Kamera**: Live-Vorschau, IR-LED-Helligkeit, Stream-URL
  - **Zeitplan**: Schlafintervall, Nachtfenster, Akkuspannung, Mindestspannungswarnung
  - **Mail**: SMTP-Konfiguration, Verbindungstest, Schnappschuss-/Protokoll-Optionen
  - **System**: WLAN, Diagnoseprotokoll (Download/Löschen), OTA-Firmware-Update
- **AP-Fallback**: Kein WLAN → Kamera öffnet eigenen Hotspot `sopobihocam` (192.168.4.1) zur Erstkonfiguration
- **Tägliche Status-E-Mail** morgens: Akkuspannung, Verbindungsanzahl, optional JPEG-Schnappschuss + Protokoll-Anhang
- **Akkuwarnung** per E-Mail bei Unterschreitung einer konfigurierbaren Mindestspannung; bei Unterspannung direkt in Nachtschlaf statt Kurzschlafzyklen
- **Diagnoseprotokoll**: LittleFS-Ringspeicher (32 KB), Zeitstempel mit NTP-Wanduhrzeit, herunterladbar über Web-UI
- **OTA-Firmware-Update** direkt über den Browser mit Fortschrittsbalken
- **mDNS**: erreichbar als `sopobihocam.local` und `sopobihocam.fritz.box`
- **Viewer-App** für Home Assistant (`sopobihocam.html`): Countdown, Stream, Akkustand, Konfigurationslink

---

## Hardware

| Komponente | Details |
|---|---|
| Mikrocontroller | Freenove ESP32-S3-WROOM CAM (N16R8: 16 MB Flash, 8 MB PSRAM) |
| Kamera | OV2640 NoIR (kein IR-Sperrfilter → Nachtaufnahmen möglich) |
| Akku | Li-Ion 2500 mAh |
| Solar-Laderegler | Raspberry Pi Solar Power Manager Module D (6–24 V → 5 V/3 A) |
| Solarmodul | 330×200 mm, Vmpp = 18 V, Impp = 0,6 A (~10,8 W) |
| IR-LED | BC547-NPN-Transistor an GPIO14 (LEDC PWM) |
| RGB-LED | WS2812 an GPIO48 |
| Akkumessung | GPIO3 (ADC1_CH2), 100 kΩ/100 kΩ Spannungsteiler, kalibriert |

### Pin-Belegung

| Funktion | GPIO |
|---|---|
| Kamera XCLK | 15 |
| Kamera I2C SDA/SCL | 4 / 5 |
| Kamera Datenleitungen | 11, 9, 8, 10, 12, 18, 17, 16 |
| Kamera VSYNC/HREF/PCLK | 6 / 7 / 13 |
| IR-LED (BC547) | 14 |
| RGB-LED (WS2812) | 48 |
| Akku-ADC | 3 |

### Stromverbrauch (gemessen)

| Betriebszustand | Strom |
|---|---|
| Deep Sleep | ~23 mA (Kameramodul ~20 mA + Power-LED ~3 mA, nicht abschaltbar) |
| Aktiv (WLAN + Kamera + Stream) | ~180 mA |
| Theoretische Akkulaufzeit (ohne Solar) | ~4,5 Tage |

---

## Zustandsautomat

```
BOOT → Akkumessung
  ├─ Akku niedrig & !mail_sent → WLAN → NTP → Warnmail → Nachtschlaf
  ├─ Akku niedrig & mail_sent  → Nachtschlaf
  └─ Akku OK → Kamera-Init → WLAN (STA) → NTP → Morgen-Mail (1×/Tag) → Bereit
                                       └─ Timeout → AP 192.168.4.1 → Bereit

Bereit:
  ├─ Kein Client nach wait_for_client_s + Nacht → Nachtschlaf (bis sleep_start_h)
  ├─ Kein Client nach wait_for_client_s + Tag   → Deep Sleep (sleep_interval_s)
  └─ Stream-Client → wach bis Trennung → Deep Sleep

Nachtschlaf → Aufwachen um sleep_start_h
Deep Sleep  → Aufwachen nach sleep_interval_s Sekunden
```

---

## URLs

| Funktion | Adresse |
|---|---|
| Konfigurationsseite | `http://sopobihocam.fritz.box/` |
| MJPEG-Stream | `http://sopobihocam.fritz.box:81/stream` |
| JPEG-Snapshot | `http://sopobihocam.fritz.box:81/jpg` |
| Akkuspannung (JSON) | `http://sopobihocam.fritz.box/battery` |
| Viewer-App (Home Assistant) | `http://homeassistant.local:8123/local/sopobihocam.html` |

---

## Software / Build

- **Arduino IDE** 2.3.5
- **ESP32 Board-Paket** 3.3.7
- **Partition Scheme**: Default 8MB with spiffs (OTA-fähig)
- **PSRAM**: OPI PSRAM aktiviert (`PSRAM=opi`)

### Abhängigkeiten

| Bibliothek | Version | Quelle |
|---|---|---|
| ReadyMail | 0.3.8 | Arduino Library Manager (Mobizt) |
| esp_camera, WiFi, WebServer, Preferences, ESPmDNS, LittleFS, Update | — | ESP32 Arduino Core 3.3.7 |

### Kompilieren (nur Binary für OTA)

```bat
"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" ^
  compile ^
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB" ^
  --libraries "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\libraries" ^
  --output-dir "...\Sopobihocam\build" ^
  "...\Sopobihocam"
```

Binary danach unter `build\Sopobihocam.ino.bin` → Upload über Web-UI (System-Tab → OTA).

---

## Projektstruktur

| Datei | Inhalt |
|---|---|
| `Sopobihocam.ino` | Hauptsketch: `setup()` Zustandsautomat, `loop()` Sleep-Logik |
| `config.h` | Pin-Definitionen, Konstanten, Config-Struct |
| `camera_manager.ino` | OV2640-Init mit Retries, Sensor-Einstellungen, AE-Warmup |
| `stream_server.cpp` | esp_http_server Port 81: `/stream` (MJPEG) + `/jpg` (Snapshot) |
| `web_ui.ino` | WebServer Port 80: Konfigurations-UI, AJAX-Endpoints, OTA |
| `wifi_manager.ino` | STA-Verbindung, AP-Fallback, mDNS |
| `time_manager.ino` | NTP-Sync, RTC-Zeitakkumulator, `isNightTime()`, `secondsUntilMorning()` |
| `sleep_manager.ino` | `shutdownAndSleep()`, `enterDeepSleep()`, `enterNightSleep()` |
| `health_check.ino` | Tägliche Morgen-Mail mit optionalem Snapshot und Log |
| `mail_sender.cpp` | ReadyMail SMTP: Warnmail, Health-Mail, Verbindungstest |
| `battery.ino` | ADC-Spannungsmessung (Median aus 16 kalibrierten Samples) |
| `nvs_manager.ino` | Preferences-basierte Konfiguration (laden/speichern) |
| `logger.ino` | LittleFS-Ringspeicher-Logger mit HTTP-Endpoints |
| `ir_led.ino` | LEDC-PWM für IR-LED |
| `sopobihocam.html` | Viewer-App für Home Assistant |

---

## Lizenz

Privates Projekt – kein offizieller Lizenzrahmen. Freie Nutzung für private, nicht-kommerzielle Zwecke.
