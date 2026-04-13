// ============================================================
// WiFi Manager – STA-Verbindung, AP-Fallback, mDNS
// ============================================================

#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>

// STA-Verbindung versuchen (blockiert bis timeout)
bool connectSTA(unsigned long timeout_ms) {
  if (cfg.wifi_ssid.length() == 0) {
    logLine("[WIFI] Keine SSID konfiguriert\n");
    return false;
  }

  logLine("[WIFI] Verbinde mit '%s'...\n", cfg.wifi_ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);  // DHCP-Hostname → FritzBox-DNS: sopobihocam.fritz.box
  WiFi.setSleep(false);  // Power-Save vor dem Start deaktivieren (wie Freenove-Referenzsketch)
  WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) {
      logLine("[WIFI] STA-Timeout nach %lums\n", millis() - start);
      WiFi.disconnect();
      return false;
    }
    delay(500);
    Serial.print(".");   // Fortschrittspunkte nur auf Serial (zu häufig für Log)
  }
  Serial.println();
  logLine("[WIFI] STA verbunden nach %lums, IP: %s\n",
          millis() - start, WiFi.localIP().toString().c_str());
  esp_wifi_set_ps(WIFI_PS_NONE);  // Power-Save aus → keine ACK-Verzögerungen im Stream
  return true;
}

// Access Point starten
void startAP() {
  logLine("[WIFI] Starte Access Point...\n");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID);
  apMode = true;
  rgbBlinking = true;  // RGB blinkt hell rot: kein WiFi, Konfiguration nötig
  esp_wifi_set_ps(WIFI_PS_NONE);  // Power-Save aus → keine ACK-Verzögerungen im Stream
  logLine("[WIFI] AP gestartet, SSID: %s, IP: %s\n",
          AP_SSID, WiFi.softAPIP().toString().c_str());
}

// mDNS-Dienst starten
void setupMDNS() {
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    logLine("[MDNS] Hostname: %s.local\n", MDNS_HOSTNAME);
  } else {
    logLine("[MDNS] Fehler beim Start\n");
  }
}
