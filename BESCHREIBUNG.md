# Sopobihocam – Projektbeschreibung

## Überblick

Sopobihocam (Solar Powered Bird House Camera) ist eine energieeffiziente Nistkastenüberwachung für den Außeneinsatz. Das System basiert auf einem **Freenove ESP32-S3-WROOM**-Board mit **OV2640 NoIR**-Kameramodul und ist für den Dauerbetrieb mit Solarladung und Lithium-Akku ausgelegt.

## Kernprinzip: On-Demand-Betrieb

Die Kamera ist nicht dauerhaft eingeschaltet. Sie wacht in einstellbaren Intervallen (Standard: 60 Sekunden) kurz auf und wartet dann eine konfigurierbare Zeit (Standard: 15 Sekunden) darauf, dass sich ein Browser verbindet. Kommt keine Verbindung, schläft sie sofort wieder ein. Verbindet sich ein Client, bleibt sie so lange aktiv wie der Stream läuft. Dieses Konzept minimiert den Stromverbrauch deutlich.

## Nachtruhe

Zwischen konfigurierbaren Uhrzeiten (Standard: 22:00–08:00 Uhr) schläft die Kamera die gesamte Nacht in einem einzigen Schlafzyklus durch – ohne wiederholtes Aufwachen. Die Zeit wird per NTP synchronisiert (Zeitzone Deutschland, automatische Sommerzeit) und über Deep-Sleep-Zyklen hinweg aus dem RTC-Speicher rekonstruiert.

## Tägliche Status-E-Mail

Beim ersten Aufwachen nach der Nachtruhe sendet die Kamera automatisch eine Morgen-Mail mit aktueller Akkuspannung und Anzahl der Verbindungen seit der letzten Mail. Optional können ein JPEG-Schnappschuss und das Diagnoseprotokoll als Anhänge beigefügt werden – beides einzeln per Checkbox in der Konfiguration einstellbar.

## Funktionsübersicht

- **MJPEG-Live-Stream** per Browser, ohne App oder Sondersoftware
- **Tab-basierte Konfigurationswebseite** mit 4 Reitern: Kamera, Zeitplan, Mail, System
- **Nachtmodus**: automatischer Langschlaf von `sleep_end_h` bis `sleep_start_h`, NTP-basiert
- **NTP-Zeitsynchronisation** (Zeitzone Deutschland, inkl. automatische Sommerzeit)
- **IR-LED** (GPIO14, PWM-gesteuert) für Nachtaufnahmen, Helligkeit per Schieberegler einstellbar
- **Tägliche Status-E-Mail** morgens mit Akkuspannung, Verbindungsanzahl, optionalem Schnappschuss und optionalem Protokoll
- **Akkuwarnung** per E-Mail bei Unterschreitung einer konfigurierbaren Mindestspannung; bei Unterspannung geht die Kamera direkt in den Nachtschlaf statt in Kurzschlafzyklen
- **AP-Fallback**: Kein WLAN? Die Kamera öffnet einen eigenen Hotspot (192.168.4.1) zur Erstkonfiguration
- **Diagnoseprotokoll**: Alle Ereignisse werden mit Datum und Uhrzeit in einer Datei auf dem internen Dateisystem gespeichert (max. 200 KB, automatische Rotation); herunterladbar über die Konfigurationsseite
- **Reset-Diagnose**: Nach unerwartetem Neustart wird der genaue Grund im Log festgehalten (Brownout, Watchdog, Panic etc.)
- **OTA-Firmware-Update** direkt über den Browser (System-Tab), mit Fortschrittsbalken und automatischem Seiten-Reload nach erfolgreichem Update
- **Firmware-Versionsanzeige** im System-Tab (Kompilierungsdatum/-uhrzeit)
- **Viewer-App** für Home Assistant: zeigt Countdown mit einstellbarem Schlafintervall, Stream, Akkustand und Konfigurationslink

## Hardware

| Komponente | Details |
|---|---|
| Mikrocontroller | Freenove ESP32-S3-WROOM CAM (N16R8: 16 MB Flash, 8 MB PSRAM) |
| Kamera | OV2640 NoIR (kein IR-Sperrfilter → Nachtaufnahmen möglich) |
| Akku | Li-Ion 2500 mAh |
| Solar-Laderegler | Raspberry Pi Solar Power Manager Module D (6–24 V → 5 V/3 A) |
| Solarmodul | 330×200 mm, Vmpp = 18 V, Impp = 0,6 A (~10,8 W) |
| IR-LED | BC547-NPN-Transistor an GPIO14 |
| RGB-LED | WS2812 an GPIO48 |
| Akkumessung | GPIO3 (ADC), 100 kΩ/100 kΩ Spannungsteiler |

## Stromverbrauch (gemessen)

| Betriebszustand | Strom |
|---|---|
| Deep Sleep | ~23 mA (Kameramodul ~20 mA + Power-LED ~3 mA, nicht abschaltbar) |
| Aktiv (WLAN + Stream) | ~180 mA |
| Theoretische Akkulaufzeit (ohne Solar) | ~4,5 Tage |

## Software

- **Plattform**: Arduino IDE 2.3.5, ESP32-Board-Paket 3.3.7, Partition scheme: Default 8MB with spiffs
- **Hauptbibliotheken**: ReadyMail 0.3.8 (Mobizt) für SMTP-Mailversand, LittleFS für Diagnoseprotokoll
- **Zustand nach Neustart**: Lädt Konfiguration aus NVS, misst Akkuspannung, initialisiert Kamera, verbindet WLAN, synchronisiert Zeit per NTP, sendet ggf. Morgen-Statusmail
