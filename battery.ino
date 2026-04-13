// ============================================================
// Battery voltage measurement via ADC with voltage divider.
// Uses analogReadMilliVolts() which applies ESP32 factory ADC
// calibration (curve fitting from eFuse) if available.
// ============================================================

// Battery voltage measurement (median of multiple calibrated samples)
float measureBatteryVoltage() {
  analogSetAttenuation(ADC_11db);  // 0–3.3V range
  analogReadResolution(12);        // 12-bit (0–4095)

  // Collect calibrated millivolt readings
  uint32_t samples[BATT_ADC_SAMPLES];
  for (int i = 0; i < BATT_ADC_SAMPLES; i++) {
    samples[i] = (uint32_t)analogReadMilliVolts(BATT_ADC_PIN);
    delay(2);
  }

  // Sort for median (cmpUint32 defined in sleep_manager.ino)
  qsort(samples, BATT_ADC_SAMPLES, sizeof(uint32_t), cmpUint32);
  uint32_t medianMv = samples[BATT_ADC_SAMPLES / 2];

  // Calibrated pin voltage (+ fixed ADC offset) → battery voltage via measured divider ratio
  float pinVoltage  = (medianMv + BATT_ADC_OFFSET_MV) / 1000.0f;
  float battVoltage = pinVoltage * BATT_DIVIDER_RATIO;

  logLine("[BATT] ADC median: %umV (kalibriert), Pin: %.3fV, Akku: %.3fV\n",
          medianMv, pinVoltage, battVoltage);
  return battVoltage;
}
