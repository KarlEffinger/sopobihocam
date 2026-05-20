#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Sopobihocam – Konfiguration und Pin-Definitionen
// Board: Freenove ESP32-S3-WROOM CAM mit OV2640 NoIR
// ============================================================

#include <esp_camera.h>

// --- Kamera-Pins (Freenove ESP32-S3-WROOM) ---
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD     4
#define CAM_PIN_SIOC     5

#define CAM_PIN_D0      11
#define CAM_PIN_D1       9
#define CAM_PIN_D2       8
#define CAM_PIN_D3      10
#define CAM_PIN_D4      12
#define CAM_PIN_D5      18
#define CAM_PIN_D6      17
#define CAM_PIN_D7      16

#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_PCLK    13

// --- Batterie-ADC ---
#define BATT_ADC_PIN        3       // GPIO3, ADC1_CH2 (funktioniert bei WiFi)
#define BATT_DIVIDER_RATIO  2.0143f // actual measured divider ratio: Vbatt(4.083V) / Vpin(2.027V)
#define BATT_ADC_OFFSET_MV  27      // fixed ADC offset after calibration: Vpin_multimeter(2027mV) - Vpin_ADC(2000mV)
#define BATT_ADC_SAMPLES   16       // Anzahl ADC-Messungen für Mittelwert
#define BATT_NO_BATTERY_V  1.0f     // Unter diesem Wert: kein Akku → USB-Betrieb

// --- Onboard-LEDs ---
#define LED_BLUE_PIN      2
#define LED_RGB_PIN      48   // WS2812 RGB-LED

// --- IR-LED (BC547 NPN-Transistor, leuchtet wenn Kamera aktiv) ---
#define IR_LED_PIN            14
#define IR_LED_PWM_FREQ     1000
#define IR_LED_PWM_BITS        8

// --- Timing-Konstanten ---
#define WIFI_STA_TIMEOUT_MS   60000
#define MAIL_RETRY_DELAY_MS   10000
#define MAIL_MAX_RETRIES          3
#define CAMERA_MAX_RETRIES        3

// --- NVS-Namespace ---
#define NVS_NAMESPACE  "sopobihocam"

// --- Firmware-Version (Kompilierzeit) ---
#define FIRMWARE_VERSION  (__DATE__ " " __TIME__)

// --- Logger ---
#define LOG_MAX_SIZE  (200 * 1024)  // 200 KB Ringspeicher
void logLine(const char* format, ...);

// --- Default-Werte ---
#define DEFAULT_WAIT_FOR_CLIENT_S   15    // Sekunden warten auf ersten Client
#define DEFAULT_SLEEP_INTERVAL_S    60    // Sekunden schlafen zwischen Checks
#define DEFAULT_BATT_MIN_V      3.3f
#define DEFAULT_MAIL_PORT       465
#define DEFAULT_SLEEP_START_H    8        // Erste aktive Stunde (Uhr)
#define DEFAULT_SLEEP_END_H     22        // Letzte aktive Stunde (Uhr)
#define DEFAULT_MAIL_SNAPSHOT  true       // Schnappschuss standardmäßig mitsenden
#define DEFAULT_MAIL_LOG       false      // Protokoll standardmäßig nicht mitsenden
#define DEFAULT_MODE            0         // 0 = täglich, 1 = aktiv
#define DEFAULT_IMAP_HOST       "imap.strato.de"
#define DEFAULT_IMAP_PORT       993
#define DEFAULT_IMAP_CMD        "sopobihocam:aktiv"

// --- AP-Konfiguration ---
#define AP_SSID          "sopobihocam"
#define AP_IP            IPAddress(192, 168, 4, 1)
#define AP_GATEWAY       IPAddress(192, 168, 4, 1)
#define AP_SUBNET        IPAddress(255, 255, 255, 0)
#define MDNS_HOSTNAME    "sopobihocam"

// --- MJPEG-Stream ---
#define STREAM_BOUNDARY  "frame"

// --- Konfigurationsstruktur ---
struct Config {
  String   wifi_ssid;
  String   wifi_pass;
  uint16_t wait_for_client_s;   // Sekunden warten auf ersten Client (5–120)
  uint16_t sleep_interval_s;    // Sekunden schlafen zwischen Checks (10–3600)
  float    batt_min_v;
  String   mail_host;
  uint16_t mail_port;
  String   mail_user;
  String   mail_pass;
  String   mail_to;
  bool     mail_sent_flag;
  uint8_t  led_brightness;      // 0–100 %
  uint8_t  sleep_start_h;       // Erste aktive Stunde (0–23), z.B. 8
  uint8_t  sleep_end_h;         // Letzte aktive Stunde (0–23), z.B. 22
  bool     mail_snapshot;       // JPEG-Schnappschuss an Health-Mail anhängen
  bool     mail_log;            // Protokoll (log.txt) an Health-Mail anhängen
  uint8_t  mode;                // 0 = täglich (1× pro Tag), 1 = aktiv (normales Polling)
  String   imap_host;           // IMAP-Server für Befehlsempfang
  uint16_t imap_port;           // IMAP-Port (993 = SSL)
  String   imap_cmd;            // Betreff-String zum Umschalten auf Modus 1
};

// --- Globale Variablen (extern) ---
extern Config cfg;
extern bool rgbBlinking;
extern volatile bool streamClientConnected;

// --- Mail-Funktionen (mail_sender.cpp) ---
bool sendWarningMail(float batt_v);
bool sendHealthMail(float batt_v, uint32_t conn_count,
                    const uint8_t *jpg_buf, size_t jpg_len,
                    const uint8_t *log_buf, size_t log_len);
bool testMailConnection(const String &host, int port,
                        const String &user, const String &pass,
                        String &errorMsg);

// --- IMAP-Funktionen (imap_receiver.cpp) ---
bool checkImapForCommand();

// --- RTC-Variablen ---
extern uint32_t rtc_conn_count;      // health_check.ino, auch in stream_server.cpp
extern uint32_t rtc_ntp_accum_s;     // time_manager.ino, auch in sleep_manager.ino
extern WebServer server;
extern unsigned long lastClientActivity;
extern bool apMode;
extern uint8_t ledCurrentPct;

#endif // CONFIG_H
