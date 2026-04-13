# Sopobihocam – Anleitung

## 1. Erstinbetriebnahme

### Firmware hochladen (USB)

1. Arduino IDE 2.3.5 öffnen
2. Board an USB anschließen (COM8)
3. Serial Monitor **schließen**
4. Kompilieren und hochladen mit:

```
"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB" --libraries "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\libraries" --upload --port COM8 "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam"
```

> **Hinweis Pfade:** Auf dem zweiten Rechner `<username>` durch den eigenen Windows-Benutzernamen ersetzen.

> **Hinweis:** Falls die Kamera im Deep Sleep ist, ist COM8 nicht verfügbar.  
> Bootloader-Modus: **BOOT** gedrückt halten → **RESET** kurz drücken → **BOOT** loslassen → Upload starten.

---

## 2. WLAN konfigurieren (Erstkonfiguration)

Beim ersten Start (oder wenn kein WLAN hinterlegt ist) öffnet die Kamera einen eigenen Hotspot:

1. Mit dem Hotspot **„Sopobihocam"** verbinden
2. Browser öffnen: `http://192.168.4.1`
3. Im Tab **System** → SSID und Passwort des Heimnetzwerks eintragen
4. **Speichern** → Kamera startet neu und verbindet sich mit dem WLAN

> Die RGB-LED blinkt hell rot im AP-Modus. Nach erfolgreicher WLAN-Verbindung leuchtet sie kurz grün.

---

## 3. Konfigurationsseite aufrufen

Nach erfolgreicher WLAN-Verbindung ist die Kamera unter folgenden Adressen erreichbar:

- `http://sopobihocam.fritz.box` (FritzBox-Heimnetz)
- `http://sopobihocam.local` (mDNS)

Die Konfigurationsseite ist nur erreichbar, solange die Kamera wach ist (d.h. kurz nach einem Aufwachen, während ein Client verbunden ist, oder beim ersten Öffnen der Seite – der eingebaute Heartbeat hält die Kamera dann wach).

---

## 4. Konfigurationsoptionen

Die Konfigurationsseite ist in vier Tabs unterteilt. Die Tab-Leiste bleibt beim Scrollen oben sichtbar.

### Tab: Kamera
| Element | Beschreibung |
|---|---|
| Live-Vorschau | JPEG-Vorschaubild der Kamera (Polling alle 100 ms) |
| IR-LED Helligkeit | 0 = aus, 1–100 = Helligkeit in % (sofortige Wirkung, wird gespeichert) |
| Stream-URL | Link zum MJPEG-Stream auf Port 81 |

### Tab: Zeitplan
| Feld | Beschreibung | Standard |
|---|---|---|
| Wartezeit auf ersten Client | Sekunden, die die Kamera nach dem Aufwachen auf einen Browser wartet (5–120 s) | 15 s |
| Schlafintervall | Zeit zwischen zwei Aufwachversuchen (10–3600 s) | 60 s |
| Aktiv ab (Uhr) | Erste Stunde, ab der die Kamera normal aufwacht (0–23) | 8 |
| Aktiv bis (Uhr) | Letzte Stunde, nach der die Kamera bis zum Morgen durchschläft (0–23) | 22 |
| Mindestspannung | Spannung, unterhalb derer eine Warnung per E-Mail gesendet wird (V) | 3,3 V |

> **Hinweis:** `Aktiv ab` muss kleiner als `Aktiv bis` sein, sonst werden die Standardwerte 8 und 22 verwendet.

Die angezeigte Uhrzeit zeigt die aktuell per NTP geschätzte Lokalzeit (inkl. Sommerzeit).

- **„Batteriewarnung zurücksetzen"**: Schaltfläche erscheint nur, wenn die Warnung bereits gesendet wurde. Erlaubt erneuten Versand nach dem Aufladen.

### Tab: Mail
| Feld | Beschreibung |
|---|---|
| SMTP-Host | Adresse des Mailservers (z.B. `smtp.gmail.com`) |
| Port | SMTP-Port (z.B. 465 für SSL) |
| Benutzer | E-Mail-Adresse des Absenders |
| Passwort | App-Passwort oder SMTP-Passwort |
| Empfänger | Zieladresse für Warnungen und Status-E-Mails |
| Schnappschuss mitsenden | JPEG-Bild als Anhang der täglichen Morgen-Mail beifügen (Standard: an) |
| Protokoll mitsenden | Diagnoseprotokoll (`log.txt`) als Anhang der Morgen-Mail beifügen (Standard: aus) |

- **„Verbindung testen"**: Testet SMTP-Verbindung ohne eine Mail zu senden

### Tab: System
| Element | Beschreibung |
|---|---|
| Firmware-Version | Zeigt Kompilierungsdatum und -uhrzeit der installierten Firmware |
| SSID / Passwort | WLAN-Zugangsdaten |
| Log-Datei | Aktuelle Größe, Download und Löschen (mit Bestätigungsdialog) |
| Firmware-Update (OTA) | `.bin`-Datei auswählen und per Browser hochladen |

---

## 5. Stream anschauen

### Direkt im Browser
```
http://sopobihocam.fritz.box:81/stream
```

### Über die Viewer-App (Home Assistant)
Die Datei `sopobihocam.html` auf Home Assistant unter `/config/www/` ablegen.  
Aufruf: `http://homeassistant.local:8123/local/sopobihocam.html`

Die App:
- Zeigt einen Countdown, solange die Kamera schläft; das **Schlafintervall** ist oben einstellbar (Standard: 120 s, bleibt nach Reload gespeichert)
- Verbindet sich automatisch sobald die Kamera aufwacht
- Zeigt Akkuspannung, Link zur Konfigurationsseite und „Verlassen"-Button
- Benennt den Browser-Tab zu **„live - SoPoBiHoCam"** sobald der Stream läuft
- Versucht nach einem Verbindungsabbruch automatisch neu zu verbinden

---

## 6. Betriebsablauf verstehen

```
Aufwachen
  │
  ├─ Akkuspannung messen
  │     └─ Zu niedrig (und kein USB)?
  │           ├─ Noch keine Warnung gesendet → WLAN → NTP → Warn-Mail → Nachtschlaf
  │           └─ Warnung bereits gesendet    → direkt Nachtschlaf (schont Akku)
  │
  ├─ Kamera initialisieren
  ├─ WLAN verbinden → NTP-Zeitsynchronisation
  │     └─ Kein WLAN → AP-Hotspot öffnen
  │
  ├─ Tageszeit erkannt (nach sleep_start_h)?
  │     └─ Ja → Morgen-Status-Mail senden (einmal pro Tag, wenn konfiguriert)
  │
  ├─ Warten auf Browser-Client (wait_for_client_s)
  │     ├─ Client verbindet → Stream läuft → wach bis Client trennt
  │     └─ Kein Client → Nachtfenster prüfen:
  │           ├─ Nacht → Nachtschlaf bis sleep_start_h
  │           └─ Tag → Kurzschlaf (sleep_interval_s)
  └─ (Schleife)
```

**Konfigurationsseite offen:** Der Browser sendet alle paar Sekunden einen Heartbeat-Ping – die Kamera bleibt wach, solange die Seite offen ist.

---

## 7. Status-E-Mail (täglich, morgens)

Beim ersten Aufwachen nach der Nachtruhe (also wenn `sleep_start_h` überschritten ist) sendet die Kamera automatisch eine Mail mit:

- Aktueller Akkuspannung
- Anzahl der Stream-Verbindungen seit der letzten Mail
- Optional: JPEG-Schnappschuss als Anhang („Schnappschuss mitsenden" im Mail-Tab)
- Optional: Diagnoseprotokoll als Anhang („Protokoll mitsenden" im Mail-Tab)

Voraussetzung: SMTP-Zugangsdaten sind konfiguriert und WLAN ist verbunden. Bei einem Fehler wird es beim nächsten Wake automatisch erneut versucht.

---

## 8. Diagnoseprotokoll

Alle Ereignisse werden mit Datum, Uhrzeit und Boot-Sekunden protokolliert:

```
[T+12s|12.04.26 08:01:15] [NTP] Sync OK: 12.04.2026 08:01:15 (DE Lokalzeit)
[T+0s|--.--.-- --:--:--]  [BOOT] Reset – Grund: Brownout
```

- **Format**: `[T+Xs|TT.MM.JJ HH:MM:SS]` – vor NTP-Sync erscheint `--.--.-- --:--:--`
- **Reset-Grund**: Bei unerwartetem Neustart wird die Ursache protokolliert (Brownout, Watchdog, Panic, Einschalten etc.)
- **Maximale Größe**: 200 KB; bei Überschreitung wird die ältere Hälfte automatisch verworfen
- **Download / Löschen**: Im System-Tab der Konfigurationsseite (Löschen mit Bestätigungsdialog)

---

## 9. RGB-LED Statusanzeige

| Farbe | Bedeutung |
|---|---|
| Blau (dauerhaft) | Kamera bootet |
| Grün (kurz, 1 s) | WLAN verbunden, betriebsbereit |
| Rot (gedimmt) | Kamera aktiv (Stream oder Konfiguration) |
| Hellrot blinkend | AP-Modus (Hotspot aktiv) |

---

## 10. Firmware-Update per OTA (Over-the-Air)

Nach der Erstkonfiguration können neue Firmware-Versionen direkt über den Browser eingespielt werden — ohne USB-Kabel.

### Voraussetzungen
- Kamera ist mit dem WLAN verbunden (STA-Modus)
- Konfigurationsseite ist geöffnet (Kamera bleibt wach)

### Firmware kompilieren

```
"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB" --libraries "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\libraries" --output-dir "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build" "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam" && del /q "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.bootloader.bin" "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.partitions.bin" "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.merged.bin" "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.elf" "C:\Users\<username>\Nextcloud\Documents\Elektronikprojekte\Arduino\Sopobihocam\build\*.map"
```

> **Hinweis Pfade:** Auf dem zweiten Rechner `<username>` durch den eigenen Windows-Benutzernamen ersetzen.

Die fertige Datei liegt danach unter: `build\Sopobihocam.ino.bin`

### Upload

1. Konfigurationsseite öffnen (`http://sopobihocam.fritz.box`)
2. Tab **System** → **Firmware-Update (OTA)**
3. `Sopobihocam.ino.bin` auswählen → **Firmware hochladen**
4. Fortschrittsbalken zeigt den Upload-Fortschritt
5. Nach erfolgreichem Upload startet die Kamera automatisch neu; der Browser lädt die Seite nach ~12 Sekunden selbstständig neu
6. Im System-Tab die angezeigte Firmware-Version prüfen, um den Erfolg zu bestätigen

> **Sicherheit:** Das ESP32-OTA-System prüft Magic Byte und MD5-Prüfsumme der Datei. Offensichtlich falsche Dateien werden vor dem Flashen abgewiesen. Die alte Firmware bleibt auf der zweiten OTA-Partition erhalten und kann per USB-Upload wiederhergestellt werden.

---

## 11. Bekannte Einschränkungen

- **Deep Sleep = kein COM8**: Während die Kamera schläft, ist der serielle Port nicht verfügbar. Für einen USB-Upload muss der Bootloader-Modus manuell aktiviert werden (s. Abschnitt 1).
- **Kein Ausschalten des Kameramoduls im Schlaf**: Das OV2640-Modul hat keinen Power-Down-Pin auf diesem Board → ~20 mA Ruhestrom auch im Deep Sleep.
- **NTP nach Power-Off weg**: Nach einem vollständigen Stromausfall (nicht Deep Sleep) geht die NTP-Zeit verloren. Die Kamera synchronisiert sie beim nächsten WLAN-Connect neu. Bis dahin erscheinen Log-Zeitstempel als `--.--.-- --:--:--` und der Nachtmodus greift erst nach der Synchronisation.
