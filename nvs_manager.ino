// ============================================================
// NVS Manager – Konfiguration laden/speichern via Preferences
// ============================================================

#include <Preferences.h>

Preferences prefs;

// Konfiguration aus NVS laden (oder Defaults setzen)
void loadConfig() {
  prefs.begin(NVS_NAMESPACE, true);  // read-only

  cfg.wifi_ssid          = prefs.getString("wifi_ssid",    "");
  cfg.wifi_pass          = prefs.getString("wifi_pass",    "");
  cfg.wait_for_client_s  = prefs.getUShort("wait_client_s", DEFAULT_WAIT_FOR_CLIENT_S);
  cfg.sleep_interval_s   = prefs.getUShort("sleep_ivl_s",   DEFAULT_SLEEP_INTERVAL_S);
  cfg.batt_min_v         = prefs.getFloat("batt_min_v",    DEFAULT_BATT_MIN_V);
  cfg.mail_host          = prefs.getString("mail_host",    "");
  cfg.mail_port          = prefs.getUShort("mail_port",    DEFAULT_MAIL_PORT);
  cfg.mail_user          = prefs.getString("mail_user",    "");
  cfg.mail_pass          = prefs.getString("mail_pass",    "");
  cfg.mail_to            = prefs.getString("mail_to",      "");
  cfg.mail_sent_flag     = prefs.getBool("mail_sent",      false);
  cfg.led_brightness     = prefs.getUChar("led_bright",    0);
  cfg.sleep_start_h      = prefs.getUChar("sleep_start_h", DEFAULT_SLEEP_START_H);
  cfg.sleep_end_h        = prefs.getUChar("sleep_end_h",   DEFAULT_SLEEP_END_H);
  cfg.mail_snapshot      = prefs.getBool("mail_snapshot",  DEFAULT_MAIL_SNAPSHOT);
  cfg.mail_log           = prefs.getBool("mail_log",       DEFAULT_MAIL_LOG);
  cfg.mode               = prefs.getUChar("mode",          DEFAULT_MODE);
  cfg.imap_host          = prefs.getString("imap_host",    DEFAULT_IMAP_HOST);
  cfg.imap_port          = prefs.getUShort("imap_port",    DEFAULT_IMAP_PORT);
  cfg.imap_cmd           = prefs.getString("imap_cmd",     DEFAULT_IMAP_CMD);

  prefs.end();

  // Werte auf erlaubten Bereich begrenzen
  if (cfg.wait_for_client_s <    5) cfg.wait_for_client_s =    5;
  if (cfg.wait_for_client_s >  120) cfg.wait_for_client_s =  120;
  if (cfg.sleep_interval_s  <   10) cfg.sleep_interval_s  =   10;
  if (cfg.sleep_interval_s  > 3600) cfg.sleep_interval_s  = 3600;
  if (cfg.sleep_start_h     >   23) cfg.sleep_start_h     = DEFAULT_SLEEP_START_H;
  if (cfg.sleep_end_h       >   23) cfg.sleep_end_h       = DEFAULT_SLEEP_END_H;
  if (cfg.sleep_start_h >= cfg.sleep_end_h) {
    cfg.sleep_start_h = DEFAULT_SLEEP_START_H;
    cfg.sleep_end_h   = DEFAULT_SLEEP_END_H;
  }
  if (cfg.mode > 1)         cfg.mode      = DEFAULT_MODE;
  if (cfg.imap_port == 0)   cfg.imap_port = DEFAULT_IMAP_PORT;
  if (cfg.imap_cmd.length() == 0) cfg.imap_cmd = DEFAULT_IMAP_CMD;

  logLine("[NVS] Konfiguration geladen\n");
}

// Gesamte Konfiguration in NVS speichern
void saveConfig() {
  prefs.begin(NVS_NAMESPACE, false);  // read-write

  prefs.putString("wifi_ssid",    cfg.wifi_ssid);
  prefs.putString("wifi_pass",    cfg.wifi_pass);
  prefs.putUShort("wait_client_s", cfg.wait_for_client_s);
  prefs.putUShort("sleep_ivl_s",   cfg.sleep_interval_s);
  prefs.putFloat("batt_min_v",    cfg.batt_min_v);
  prefs.putString("mail_host",    cfg.mail_host);
  prefs.putUShort("mail_port",    cfg.mail_port);
  prefs.putString("mail_user",    cfg.mail_user);
  prefs.putString("mail_pass",    cfg.mail_pass);
  prefs.putString("mail_to",      cfg.mail_to);
  prefs.putBool("mail_sent",      cfg.mail_sent_flag);
  prefs.putUChar("led_bright",    cfg.led_brightness);
  prefs.putUChar("sleep_start_h", cfg.sleep_start_h);
  prefs.putUChar("sleep_end_h",   cfg.sleep_end_h);
  prefs.putBool("mail_snapshot",  cfg.mail_snapshot);
  prefs.putBool("mail_log",       cfg.mail_log);
  prefs.putUChar("mode",          cfg.mode);
  prefs.putString("imap_host",    cfg.imap_host);
  prefs.putUShort("imap_port",    cfg.imap_port);
  prefs.putString("imap_cmd",     cfg.imap_cmd);

  prefs.end();
  logLine("[NVS] Konfiguration gespeichert\n");
}

// Einzelnes Flag mail_sent_flag speichern
void saveMailSentFlag(bool sent) {
  cfg.mail_sent_flag = sent;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBool("mail_sent", sent);
  prefs.end();
}

// LED-Helligkeit speichern
void saveLedBrightness(uint8_t pct) {
  cfg.led_brightness = pct;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUChar("led_bright", pct);
  prefs.end();
}
