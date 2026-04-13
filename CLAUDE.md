# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Sopobihocam (Solar Powered Bird House Camera) — ESP32-S3 CAM firmware (OV2640 NoIR camera) that wakes from deep sleep every `sleep_interval_s` seconds (default 60s), waits up to `wait_for_client_s` seconds (default 15s) for a browser to connect, streams MJPEG video on demand, provides a tab-based configuration web UI, monitors battery voltage, sends warning emails via SMTP when battery is low, sends daily health-check emails with optional snapshot and log attachment, supports configurable night sleep windows, and offers OTA firmware updates via browser.

## Hardware

- Freenove ESP32-S3-WROOM CAM board with OV2640 NoIR camera module
- Li-Ion 2500mAh battery, solar charging via "Raspberry Pi Solar Power Manager Module D" (6–24V input, 5V/3A output)
- ESP32 powered from the 5V output of the Solar Power Manager; BATT+ terminal connected directly to GPIO3 voltage divider for battery measurement
- Battery voltage measured via ADC (GPIO3) through a 100kΩ/100kΩ voltage divider → two-parameter calibration: BATT_ADC_OFFSET_MV=27, BATT_DIVIDER_RATIO=2.0143 (verified: (2000+27)/1000×2.0143 = 4.083V ✓)
- Uses `analogReadMilliVolts()` (ESP32 Arduino core 3.x) for factory eFuse ADC calibration
- RGB LED: WS2812 on GPIO48 — blue=booting, green=WiFi OK (briefly), red=streaming, bright red blink=AP mode
- Blue LED: GPIO2 (onboard, unused)
- Onboard green power LED (hardwired, cannot be disabled, ~3mA)
- IR LED: BC547 NPN transistor on GPIO14 (PWM via LEDC); lights up when camera is active; brightness adjustable via web UI
- **Note:** CAM_PIN_PWDN = -1 (not routed on this board) → camera module cannot be powered down in deep sleep → ~20mA quiescent current from camera module

## Build

Arduino IDE 2.3.5, ESP32 board package 3.3.7. Partition scheme: Default 8MB with spiffs (OTA-fähig).

**Achtung:** Die Pfade enthalten `kaeff` (Haupt-Rechner). Auf dem zweiten Rechner lautet der Benutzername `KRKL` → alle `kaeff` in den Befehlen durch `KRKL` ersetzen.

**Compile only** (für OTA-Upload – erzeugt nur die `.bin` im `build/`-Ordner):

```
"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB" --libraries "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\libraries" --output-dir "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build" "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam" && del /q "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.bootloader.bin" "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.partitions.bin" "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.merged.bin" "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.elf" "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.map"
```

Binary liegt danach unter: `build\Sopobihocam.ino.bin`

**Compile + USB-Upload** (nur wenn Gerät nicht im Deep Sleep, Serial Monitor geschlossen, COM8):

```
"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB" --libraries "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\libraries" --upload --port COM8 "C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam"
```

**Note:** COM8 is not available while the device is in deep sleep. To enter bootloader mode before upload:
Hold BOOT → press and release RESET → release BOOT → start upload.

## Architecture

The firmware follows a state machine:

```
BOOT → MeasureBatt
  ├─ batt low & !mail_sent → WiFi → NTP → SendMail → NightSleep (bis sleep_start_h)
  ├─ batt low &  mail_sent → NightSleep (bis sleep_start_h)
  └─ batt OK → CameraInit → TryWiFi (STA) → NTP → HealthMail (1×/Tag) → Ready
                                          └─ Timeout 60s → StartAP(192.168.4.1) → Ready
Ready:
  ├─ Serve config web UI on port 80 (STA or AP mode, 4 tabs)
  ├─ MJPEG stream on port 81 /stream
  ├─ JPEG snapshot on port 81 /jpg (liveness check for sopobihocam.html, web UI preview)
  ├─ No client within wait_for_client_s + isNightTime() → NightSleep
  ├─ No client within wait_for_client_s + !isNightTime() → DeepSleep
  └─ Stream client connects → awake indefinitely → client disconnects → DeepSleep
DeepSleep → wake after sleep_interval_s seconds (default 60s)
NightSleep → wake at sleep_start_h (default 08:00), resets health_mail_sent_today flag
```

### Key subsystems
- **NVS configuration**: wifi_ssid, wifi_pass, wait_for_client_s, sleep_interval_s, batt_min_v, mail_host, mail_port, mail_user, mail_pass, mail_to, mail_sent_flag, led_brightness, sleep_start_h, sleep_end_h, mail_snapshot, mail_log
- **Web UI**: Tab-based configuration page on port 80 (AP 192.168.4.1 or STA) with 4 tabs:
  - **Kamera**: live JPEG preview (polling /jpg), IR LED brightness slider, stream URL
  - **Zeitplan**: wait_for_client_s, sleep_interval_s, night sleep window (sleep_start_h / sleep_end_h), current NTP time display, battery voltage, batt_min_v, reset battery warning
  - **Mail**: SMTP configuration, mail_snapshot/mail_log checkboxes, connection test
  - **System**: WiFi SSID/password, log diagnostics (download/clear/size), OTA firmware update with progress bar
  - Heartbeat ping every `wait_for_client_s/3` ms keeps camera awake while config page is open
- **Sleep logic**:
  - `streamClientConnected = true` while MJPEG client is connected → no sleep
  - `lastClientActivity` reset by web UI heartbeat (/ping) → no sleep while config page open
  - If `!streamClientConnected` and `millis() - lastClientActivity > wait_for_client_s`:
    - Night time (h >= sleep_end_h or h < sleep_start_h) → `enterNightSleep()` (long sleep until sleep_start_h)
    - Day time → `enterDeepSleep()` (short sleep for sleep_interval_s)
  - Low battery → always `enterNightSleep()` (prevents drain through 60s short cycles)
- **NTP time management** (`time_manager.ino`): NTP sync via `configTzTime()` with CET/CEST timezone. Time survives deep sleep via two RTC variables: `rtc_ntp_unix_ts` (boot-normalized UTC timestamp) + `rtc_ntp_accum_s` (accumulated seconds across sleep cycles). Formula: `getEstimatedTime() = rtc_ntp_unix_ts + rtc_ntp_accum_s + millis()/1000`. **Wichtig:** ESP32-S3 interner RC-Oszillator driftet ±5% → bei 14h Nachtschlaf bis zu 42 Minuten. `syncNTP()` setzt Systemuhr vor dem SNTP-Start auf Epoche zurück, damit `getLocalTime()` auf die echte NTP-Antwort wartet (nicht die gedriftete RTC-Zeit akzeptiert).
- **MJPEG streaming**: Port 81 `/stream` via `esp_http_server`, adaptive pacing (delay = 100ms − tTotal; if tSend > 60ms: delay += tSend; clamped 20–500ms). Sets `streamClientConnected=true` on connect, `false` on disconnect. Increments `rtc_conn_count`.
- **JPEG snapshot**: Port 81 `/jpg` on same httpd instance as /stream. Used for browser live preview (JS polling in web UI) and liveness check in sopobihocam.html.
- **Mail**: SMTP warning when batt_v < batt_min_v; max 3 retries × 10s; mail_sent_flag in NVS prevents repeated sending; manual reset via web UI. SMTP connection test via web UI. Library: **ReadyMail 0.3.8** (Mobizt).
- **Health-check mail** (`health_check.ino`): Daily status mail sent on first morning wake after sleep_start_h. Contains battery voltage + connection count. Optional JPEG snapshot attachment (`cfg.mail_snapshot`) and log file attachment (`cfg.mail_log`). RTC-persistent variables: `rtc_conn_count` (reset after successful send), `rtc_health_mail_sent_today` (reset in `prepareForNight()`). On failure: retries on next 60s wake cycle.
- **Logger** (`logger.ino`): LittleFS ring buffer (32 KB max). Each line timestamped `[T+Xs|HH:MM:SS]` (boot seconds + NTP wall clock). Rotation: discards older half when exceeding LOG_MAX_SIZE (checked every 25 writes). HTTP endpoints: GET `/log` (download), GET `/clearlog` (delete), GET `/logsize` (JSON size). `readLogBuffer()` for mail attachment.
- **OTA firmware update**: Browser-based upload via POST `/update` in web UI (System tab). Uses `Update.h`, progress bar in JS, auto-restart on success.
- **Camera**: OV2640 NoIR, XCLK 20 MHz, VGA JPEG q12, fb_count=2 (PSRAM), up to 3 init attempts; on failure: warning mail + sleep. AE warmup: 5 frames × 300ms discarded. Settings: vflip=1, hmirror=1, aec2=0 (no night mode), gainceiling=4X.
- **IR LED**: GPIO14, BC547 NPN, PWM via LEDC (1kHz, 8-bit). Brightness 0–100% via web UI, stored in NVS. Fade-in on boot, fade-out on sleep. LEDC setup after camera init to avoid timer conflict.
- **mDNS hostname**: `sopobihocam` → `sopobihocam.local` and `sopobihocam.fritz.box`
- **WiFi hostname**: `WiFi.setHostname(MDNS_HOSTNAME)` called before `WiFi.begin()` → appears in FritzBox DNS as `sopobihocam.fritz.box`

### URLs
- `http://sopobihocam.fritz.box/` — configuration web UI (port 80)
- `http://sopobihocam.fritz.box:81/stream` — MJPEG stream (browser direct)
- `http://sopobihocam.fritz.box:81/jpg` — JPEG snapshot (liveness check, web UI preview)
- `http://homeassistant.local:8123/local/sopobihocam.html` — viewer app (hosted on HA)

Note: /stream and /jpg share one httpd instance on port 81. During streaming the httpd task is blocked by the MJPEG while-loop; /jpg is only used before streaming starts (liveness check) and for the web UI preview.

### sopobihocam.html (viewer app)
Single-page app hosted on Home Assistant at `/config/www/sopobihocam.html`.
- Uses `fetch()` to poll `/jpg` every 3s (2s timeout) to detect when camera wakes up
- Shows countdown "Nächste Verbindung in spätestens X Sekunden" while waiting
- On success: loads `/stream` in `<img>` tag, shows battery voltage (polls `/battery` every 30s), "⚙ Konfiguration" link, "✕ Verlassen" button
- On stream disconnect: shows "↺ Wieder verbinden" button + restarts countdown
- CAM_HOST configured as `sopobihocam.fritz.box`
- fetch() works from HA origin because ESP sends `Access-Control-Allow-Origin: *`

### Libraries
- `esp_camera.h`, `WiFi.h`, `WebServer.h`, `Preferences.h` (NVS), `ESPmDNS.h`, `LittleFS.h`, `Update.h` — ESP32 Arduino core 3.3.7
- **ReadyMail 0.3.8** (Mobizt) — installed in `C:\Users\kaeff\Nextcloud\Documents\Elektronikprojekte\Arduino\libraries`

### Why .cpp instead of .ino for some files
`stream_server.cpp` and `mail_sender.cpp` are deliberately `.cpp` rather than `.ino`: the Arduino preprocessor generates automatic forward declarations for `.ino` files **before** the `#include` directives, which causes types like `SMTPStatus` to be unknown. `.cpp` files are compiled without this mechanism.

## Current State (as of 2026-04-13)

### Working features
- On-demand operation: wakes every `sleep_interval_s` (default 60s), waits `wait_for_client_s` (default 15s) for browser
- MJPEG stream on port 81 (`/stream`) + JPEG snapshot (`/jpg`) on same httpd instance
- Tab-based web UI on port 80: Kamera (preview, IR LED), Zeitplan (timing, night sleep, battery), Mail (SMTP config, snapshot/log options), System (WiFi, log diagnostics, OTA)
- IR LED (GPIO14, BC547): PWM brightness 0–100%, fade-in/out, stored in NVS
- RGB LED (WS2812, GPIO48): blue=boot, green=ready (1s), red=active, bright red blink=AP mode
- Battery measurement: GPIO3 (ADC1_CH2), `analogReadMilliVolts()` + offset 27mV + ratio 2.0143, median of 16 samples; verified 4.083V
- NTP time sync with CET/CEST timezone, time survives deep sleep via RTC accumulator
- Configurable night sleep window (default 22:00–08:00): long sleep until morning instead of 60s cycles
- Daily health-check mail (first morning wake): battery voltage, connection count, optional JPEG snapshot + log attachment
- LittleFS logger: 32 KB ring buffer with timestamps, download/clear via web UI, attachable to health mail
- OTA firmware update via browser upload (System tab)
- Timer-based deep sleep (short: `sleep_interval_s`, long: until `sleep_start_h`)
- Low battery always triggers long night sleep (prevents drain through short cycles)
- PSRAM active (fqbn option: `PSRAM=opi`)
- ReadyMail 0.3.8: battery warning mail, health mail, SMTP connection test
- Viewer app (`sopobihocam.html`) on HA with auto-reconnect, battery display, config link

### Deep sleep sequence (`shutdownAndSleep()` in sleep_manager.ino)
1. `rgbBlinking = false` + `fadeLedOff()` — fades out IR LED and RGB indicator
2. `WiFi.disconnect(true)` + `WiFi.mode(WIFI_OFF)` + `esp_camera_deinit()`
3. Stop XCLK: `ledcDetach(LEDC_CHANNEL_0)` + `pinMode(CAM_PIN_XCLK, INPUT)`
4. IR LED GPIO cleanup: `ledcDetach(IR_LED_PIN)` + `pinMode(IR_LED_PIN, OUTPUT)` + `digitalWrite(IR_LED_PIN, LOW)`
5. Update `rtc_ntp_accum_s += sleep_s + millis()/1000` (NTP time accumulator)
6. `esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL)`
7. `esp_sleep_enable_timer_wakeup(sleep_s × 1_000_000)`
8. `Serial.flush()` + `esp_deep_sleep_start()`

Two entry points: `enterDeepSleep()` (short, `cfg.sleep_interval_s`) and `enterNightSleep()` (long, until `sleep_start_h`, calls `prepareForNight()` to reset health mail flag).

### Sleep behavior (important for debugging)
- **Stream connected** (`streamClientConnected=true`): camera stays awake indefinitely
- **Web UI open**: heartbeat /ping every `wait_for_client_s/3` resets `lastClientActivity` → no sleep
- **No stream, no web UI, daytime**: after `wait_for_client_s` seconds → `enterDeepSleep()`
- **No stream, no web UI, night time**: after `wait_for_client_s` seconds → `enterNightSleep()`
- **Stream disconnects**: `streamClientConnected=false`, `lastClientActivity=millis()` → sleep after `wait_for_client_s`
- **Low battery**: always `enterNightSleep()` regardless of time of day

### Pin assignment
- Battery ADC: GPIO3 (ADC1_CH2), 100kΩ/100kΩ voltage divider
- RGB LED: GPIO48 (WS2812)
- Blue LED: GPIO2 (unused)
- IR LED: GPIO14 (BC547 NPN, LEDC PWM)
- Camera XCLK: GPIO15 (LEDC_CHANNEL_0)
- Camera I2C: SIOD=GPIO4, SIOC=GPIO5

### File structure
- `Sopobihocam.ino` — main sketch: setup() state machine, loop() with sleep logic
- `config.h` — pin definitions, constants, Config struct, extern declarations
- `battery.ino` — ADC voltage measurement (median of 16 calibrated samples)
- `camera_manager.ino` — OV2640 init with retries, sensor settings, AE warmup
- `wifi_manager.ino` — STA connection, AP fallback, mDNS
- `stream_server.cpp` — esp_http_server on port 81: /stream (MJPEG) + /jpg (snapshot)
- `mail_sender.cpp` — ReadyMail SMTP: warning mail, health mail, connection test
- `sleep_manager.ino` — shutdownAndSleep(), enterDeepSleep(), enterNightSleep()
- `nvs_manager.ino` — Preferences load/save for all config fields
- `web_ui.ino` — WebServer on port 80: config page (HTML/JS), all AJAX endpoints, LED control, OTA
- `ir_led.ino` — LEDC PWM setup and duty control for IR LED
- `time_manager.ino` — NTP sync, RTC time accumulator, isNightTime(), secondsUntilMorning()
- `health_check.ino` — daily morning health mail with optional snapshot/log
- `logger.ino` — LittleFS ring buffer logger with HTTP endpoints
- `sopobihocam.html` — viewer app (hosted on Home Assistant)

### Power consumption (measured)
- Deep sleep: **~23mA** (camera module ~20mA not power-gatable + hardwired power LED ~3mA)
- Active (WiFi + camera + streaming): **~180mA**
- Battery 2500mAh → runtime on sleep alone: ~4.5 days (without solar)
- Solar panel: 330×200mm, Vmpp=18V, Impp=0.6A → ~10.8W rated power (sufficient for breeding season April–July)

## Important Rules

- Energy efficiency is critical — the device is solar/battery powered. Minimize wake time.
- HTTP without TLS is acceptable (local network), but structure code so TLS can be added later.
- On unclear requirements, ask before generating code.
- Always check installed library versions before making changes.
- Ensure Arduino IDE compatibility.
- Generate structured, commented code (German comments are acceptable).
- Before uploading: always ask user to close Serial Monitor and confirm board is connected on COM8.
