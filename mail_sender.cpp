// ============================================================
// Mail-Sender – SMTP-Warnmail bei niedriger Batteriespannung
// Verwendet ReadyMail von Mobizt (Nachfolger von ESP_Mail_Client)
//
// Als .cpp-Datei (nicht .ino), damit der Arduino-Preprocessor
// keine falschen Auto-Prototypen für SMTPStatus/SMTPMessage generiert.
// ============================================================

#include <Arduino.h>
#include <WebServer.h>           // wird von config.h benötigt (extern WebServer server)
#define ENABLE_SMTP
#define ENABLE_DEBUG
#define READYMAIL_DEBUG_PORT Serial
#define READYMAIL_TIME_SOURCE time(nullptr)  // Zeitstempel aus NTP
#include <ReadyMail.h>
#include <WiFiClientSecure.h>
#include "config.h"

// Letzter Status-Text aus Callback (für Fehlerausgabe)
static String _smtpLastMsg = "";
static bool   _smtpError   = false;

// ReadyMail-Callback: loggt SMTP-Zustand auf Serial
static void smtpCallback(SMTPStatus status) {
  if (!status.progress.available && status.text.length() > 0) {
    _smtpLastMsg = status.text;
    Serial.printf("[MAIL] %s\n", status.text.c_str());
    if (status.state < 0) _smtpError = true;
  }
}

// Interne Hilfsfunktion: Verbinden, Auth, optional Senden
// msg == nullptr → nur Verbindungstest (kein Versand)
static bool doSmtpSend(const String &host, int port,
                       const String &user, const String &pass,
                       SMTPMessage *msg) {
  _smtpError   = false;
  _smtpLastMsg = "";

  WiFiClientSecure ssl;
  ssl.setInsecure();  // Kein Zertifikat-Verify (lokales Netz)
  SMTPClient smtp(ssl);

  bool sslMode = (port == 465);  // Port 465: direkt SSL; 587: STARTTLS
  logLine("[MAIL] Verbinde mit %s:%d...\n", host.c_str(), port);
  smtp.connect(host.c_str(), port, smtpCallback, sslMode);
  if (!smtp.isConnected()) {
    logLine("[MAIL] Verbindung fehlgeschlagen: %s\n", _smtpLastMsg.c_str());
    return false;
  }

  smtp.authenticate(user.c_str(), pass.c_str(), readymail_auth_password);
  if (!smtp.isAuthenticated()) {
    logLine("[MAIL] Auth fehlgeschlagen: %s\n", _smtpLastMsg.c_str());
    return false;
  }

  if (msg == nullptr) return true;  // Verbindungstest – kein Versand

  smtp.send(*msg);
  return !_smtpError;
}

// SMTP-Warnmail senden (max. MAIL_MAX_RETRIES Versuche)
bool sendWarningMail(float batt_v) {
  if (cfg.mail_host.length() == 0 || cfg.mail_to.length() == 0) {
    logLine("[MAIL] Keine Mail-Konfiguration vorhanden\n");
    return false;
  }

  logLine("[MAIL] Sende Warnung (Batterie: %.2fV, Schwelle: %.2fV)\n",
          batt_v, cfg.batt_min_v);

  SMTPMessage msg;
  msg.headers.add(rfc822_subject, "Sopobihocam: Batteriespannung niedrig");
  msg.headers.add(rfc822_from, "Sopobihocam <" + cfg.mail_user + ">");
  msg.headers.add(rfc822_to, cfg.mail_to);

  String body = "Achtung: Die Batteriespannung der Sopobihocam ist niedrig.\r\n\r\n"
                "Aktuelle Spannung: " + String(batt_v, 2) + " V\r\n"
                "Schwellwert:       " + String(cfg.batt_min_v, 2) + " V\r\n\r\n"
                "Bitte Akku laden oder tauschen.";
  msg.text.body(body);

  for (int attempt = 1; attempt <= MAIL_MAX_RETRIES; attempt++) {
    logLine("[MAIL] Warnmail Versuch %d/%d\n", attempt, MAIL_MAX_RETRIES);
    if (doSmtpSend(cfg.mail_host, cfg.mail_port, cfg.mail_user, cfg.mail_pass, &msg)) {
      logLine("[MAIL] Warnmail erfolgreich gesendet\n");
      return true;
    }
    if (attempt < MAIL_MAX_RETRIES) delay(MAIL_RETRY_DELAY_MS);
  }

  logLine("[MAIL] Warnmail: alle Versuche fehlgeschlagen\n");
  return false;
}

// Tägliche Health-Mail senden (mit optionalem JPEG-Snapshot und/oder Protokoll-Anhang)
bool sendHealthMail(float batt_v, uint32_t conn_count,
                    const uint8_t *jpg_buf, size_t jpg_len,
                    const uint8_t *log_buf, size_t log_len) {
  if (cfg.mail_host.length() == 0 || cfg.mail_to.length() == 0) return false;

  logLine("[MAIL] Sende Health-Mail (Akku: %.2fV, Verbindungen: %u)\n",
          batt_v, conn_count);

  SMTPMessage msg;
  msg.headers.add(rfc822_subject, "Sopobihocam: täglicher Status");
  msg.headers.add(rfc822_from, "Sopobihocam <" + cfg.mail_user + ">");
  msg.headers.add(rfc822_to, cfg.mail_to);

  String body = "Sopobihocam – täglicher Status-Bericht\r\n\r\n"
                "Akkuspannung:  " + String(batt_v, 2) + " V\r\n"
                "Verbindungen:  " + String(conn_count) + " (seit letzter Mail)\r\n";
  msg.text.body(body);

  if (jpg_buf && jpg_len > 0) {
    Attachment att;
    att.filename              = "snapshot.jpg";
    att.mime                  = "image/jpeg";
    att.name                  = "snapshot.jpg";
    att.attach_file.blob      = jpg_buf;
    att.attach_file.blob_size = jpg_len;
    msg.attachments.add(att, attach_type_attachment);
  }

  if (log_buf && log_len > 0) {
    Attachment logAtt;
    logAtt.filename              = "sopobihocam_log.txt";
    logAtt.mime                  = "text/plain";
    logAtt.name                  = "sopobihocam_log.txt";
    logAtt.attach_file.blob      = log_buf;
    logAtt.attach_file.blob_size = log_len;
    msg.attachments.add(logAtt, attach_type_attachment);
  }

  if (doSmtpSend(cfg.mail_host, cfg.mail_port, cfg.mail_user, cfg.mail_pass, &msg)) {
    logLine("[MAIL] Health-Mail erfolgreich gesendet\n");
    return true;
  }
  logLine("[MAIL] Health-Mail fehlgeschlagen\n");
  return false;
}

// SMTP-Verbindung testen (kein Versand) – für Web-UI "Verbindung testen"
bool testMailConnection(const String &host, int port,
                        const String &user, const String &pass,
                        String &errorMsg) {
  if (host.length() == 0) {
    errorMsg = "Kein Host angegeben";
    return false;
  }

  Serial.printf("[MAIL] Teste Verbindung zu %s:%d\n", host.c_str(), port);

  if (doSmtpSend(host, port, user, pass, nullptr)) {
    Serial.println("[MAIL] Verbindungstest erfolgreich");
    return true;
  }

  errorMsg = _smtpLastMsg;
  errorMsg.replace("\"", "'");
  return false;
}
