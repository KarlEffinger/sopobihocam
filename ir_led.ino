// ============================================================
// IR-LED Steuerung via BC547 NPN-Transistor
//
// Schaltung (aktiv-HIGH: GPIO HIGH → LED leuchtet):
//   GPIO14 → R_Base (1kΩ) → BC547-Base
//   BC547-Collector → IR-LED-Kathode → R_LED → 3.3V (oder 5V)
//   BC547-Emitter → GND
//
// Alternativschaltung (gängiger bei 3.3V-Versorgung):
//   3.3V → R_LED → IR-LED-Anode → IR-LED-Kathode → BC547-Collector
//   BC547-Emitter → GND
//
// PWM über LEDC: Helligkeit 0–255 einstellbar (z.B. für Nacht-/Tagmodus)
// ============================================================

void irLedSetup() {
  ledcAttach(IR_LED_PIN, IR_LED_PWM_FREQ, IR_LED_PWM_BITS);
  ledcWrite(IR_LED_PIN, 0);  // sicher aus beim Start
  logLine("[IR] LED auf GPIO%d konfiguriert (PWM %dHz, %d-Bit)\n",
          IR_LED_PIN, IR_LED_PWM_FREQ, IR_LED_PWM_BITS);
}

// Vollhelligkeit – Standard wenn Kamera läuft
void irLedOn() {
  ledcWrite(IR_LED_PIN, 255);
}

// LED ausschalten (z.B. vor Deep Sleep)
void irLedOff() {
  ledcWrite(IR_LED_PIN, 0);
}

// Helligkeit 0–255 setzen (für späteren Web-UI-Slider)
void irLedSetDuty(uint8_t duty) {
  ledcWrite(IR_LED_PIN, duty);
}
