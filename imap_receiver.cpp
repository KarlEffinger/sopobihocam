// ============================================================
// IMAP-Receiver – prüft INBOX auf Befehlsmail zum Moduswechsel
//
// Als .cpp-Datei (nicht .ino), damit der Arduino-Preprocessor
// keine falschen Auto-Prototypen für IMAP-Typen generiert.
//
// Verwendet ReadyMail 0.3.8 (Mobizt) – IMAPClient.
// Verbindet mit direktem SSL (Port 993, kein STARTTLS).
// ============================================================

#include <Arduino.h>
#include <WebServer.h>           // wird von config.h benötigt (extern WebServer server)

// ReadyMail: Beide Modi aktivieren, damit dieselbe ReadyMail.h wie in
// mail_sender.cpp eingebunden wird (gemeinsame Typ-Definitionen).
//
// ReadyMail.h Zeile 175 enthält "ReadyMailClass ReadyMail;" – eine nicht-inline
// globale Definition. Da mail_sender.cpp dieselbe Definition erzeugt, käme es
// beim Linken zu "multiple definition of ReadyMail". Da imap_receiver.cpp das
// ReadyMail-Globalobjekt nicht direkt verwendet (nur IMAPClient), wird die
// Definition hier durch ein Makro umbenannt und damit unterdrückt.
#define ReadyMail _ReadyMail_imap_suppress_  // verhindert doppelte Definition
#define ENABLE_SMTP              // für SMTP-Typen (SMTPMessage in imap/Common.h benötigt)
#define ENABLE_IMAP
#define ENABLE_DEBUG
#define READYMAIL_DEBUG_PORT Serial
#define READYMAIL_TIME_SOURCE time(nullptr)
#include <ReadyMail.h>
#undef ReadyMail                 // Makro wieder aufheben
#include <WiFiClientSecure.h>
#include "config.h"

// Letzter Callback-Text (für Fehlerausgabe)
static String _imapLastMsg = "";
static bool   _imapError   = false;

// ReadyMail IMAP-Callback: loggt Status auf Serial
static void imapCallback(IMAPStatus status) {
  if (status.text.length() > 0) {
    _imapLastMsg = status.text;
    Serial.printf("[IMAP] %s\n", status.text.c_str());
    if (status.errorCode < 0) _imapError = true;
  }
}

// Prüft INBOX auf ungelesene Mails mit cfg.imap_cmd im Betreff.
// Gibt true zurück wenn mindestens ein Treffer gefunden und als
// gelesen markiert wurde. Bei Fehler: false.
bool checkImapForCommand() {
  if (cfg.imap_host.length() == 0 || cfg.mail_user.length() == 0 ||
      cfg.mail_pass.length() == 0 || cfg.imap_cmd.length() == 0) {
    logLine("[IMAP] Keine IMAP-Konfiguration vorhanden\n");
    return false;
  }

  logLine("[IMAP] Verbinde mit %s:%u als %s\n",
          cfg.imap_host.c_str(), cfg.imap_port, cfg.mail_user.c_str());

  _imapError   = false;
  _imapLastMsg = "";

  WiFiClientSecure ssl;
  ssl.setInsecure();  // Kein Zertifikat-Verify (lokales Netz / Energiesparen)
  IMAPClient imap(ssl);

  // Verbinden (direkt SSL, kein STARTTLS)
  if (!imap.connect(cfg.imap_host, cfg.imap_port, imapCallback, /*ssl=*/true)) {
    logLine("[IMAP] Verbindung fehlgeschlagen: %s\n", _imapLastMsg.c_str());
    return false;
  }

  // Authentifizieren
  if (!imap.authenticate(cfg.mail_user, cfg.mail_pass, readymail_auth_password)) {
    logLine("[IMAP] Auth fehlgeschlagen: %s\n", _imapLastMsg.c_str());
    imap.logout();
    return false;
  }

  // INBOX öffnen (read-write, damit STORE \Seen funktioniert)
  if (!imap.select("INBOX", /*readOnly=*/false)) {
    logLine("[IMAP] SELECT INBOX fehlgeschlagen\n");
    imap.logout();
    return false;
  }

  // SEARCH UNSEEN SUBJECT "cmd" — sucht ungelesene Mails mit cmd im Betreff
  // Limit: 10 Treffer, kein recentSort nötig
  String searchCriteria = "UID SEARCH UNSEEN SUBJECT \"" + cfg.imap_cmd + "\"";
  logLine("[IMAP] Suche: %s\n", searchCriteria.c_str());

  // dataCallback: nullptr reicht für reine Suche (nur UIDs interessieren uns)
  if (!imap.search(searchCriteria, 10, false, nullptr)) {
    logLine("[IMAP] SEARCH fehlgeschlagen: %s\n", _imapLastMsg.c_str());
    imap.logout();
    return false;
  }

  // Ergebnis-UIDs auslesen
  std::vector<uint32_t> &uids = imap.searchResult();
  logLine("[IMAP] Treffer: %u Nachricht(en)\n", (unsigned)uids.size());

  if (uids.empty()) {
    // Kein Treffer – sauber abmelden
    imap.logout();
    return false;
  }

  // Gefundene UIDs als SEEN markieren (UID STORE uid +FLAGS (\Seen))
  // Wir bauen eine komma-separierte UID-Liste für eine Store-Anfrage.
  String uidList = "";
  for (size_t i = 0; i < uids.size(); i++) {
    if (i > 0) uidList += ",";
    uidList += String(uids[i]);
  }
  String storeCmd = "UID STORE " + uidList + " +FLAGS (\\Seen)";
  logLine("[IMAP] Markiere als gelesen: %s\n", storeCmd.c_str());

  // sendCommand() lehnt SELECT, LOGOUT usw. ab, akzeptiert aber STORE/UID STORE
  imap.sendCommand(storeCmd, nullptr);

  imap.logout();
  logLine("[IMAP] Befehlsmail gefunden – Modus wird umgeschaltet\n");
  return true;
}
