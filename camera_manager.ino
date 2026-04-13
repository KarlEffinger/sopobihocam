// ============================================================
// Kamera-Manager – Init mit Retries, OV2640 NoIR
// ============================================================

// Kamera initialisieren (max. CAMERA_MAX_RETRIES Versuche)
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = CAM_PIN_D0;
  config.pin_d1       = CAM_PIN_D1;
  config.pin_d2       = CAM_PIN_D2;
  config.pin_d3       = CAM_PIN_D3;
  config.pin_d4       = CAM_PIN_D4;
  config.pin_d5       = CAM_PIN_D5;
  config.pin_d6       = CAM_PIN_D6;
  config.pin_d7       = CAM_PIN_D7;
  config.pin_xclk     = CAM_PIN_XCLK;
  config.pin_pclk     = CAM_PIN_PCLK;
  config.pin_vsync    = CAM_PIN_VSYNC;
  config.pin_href     = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn     = CAM_PIN_PWDN;
  config.pin_reset    = CAM_PIN_RESET;
  // 20 MHz: Standard für OV2640 (OV3660 brauchte 10 MHz)
  config.xclk_freq_hz = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_VGA;   // VGA (640x480)
  config.jpeg_quality  = 12;
  config.fb_count      = 1;
  config.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location   = CAMERA_FB_IN_PSRAM;

  if (psramFound()) {
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
    logLine("[CAM] PSRAM erkannt, VGA JPEG q12, fb_count=2, XCLK 20 MHz\n");
  } else {
    config.fb_location  = CAMERA_FB_IN_DRAM;
    logLine("[CAM] Kein PSRAM, VGA JPEG, XCLK 20 MHz\n");
  }

  // Init mit Retries
  esp_err_t err;
  for (int attempt = 1; attempt <= CAMERA_MAX_RETRIES; attempt++) {
    err = esp_camera_init(&config);
    if (err == ESP_OK) {
      logLine("[CAM] Init erfolgreich (Versuch %d)\n", attempt);

      sensor_t *s = esp_camera_sensor_get();
      if (s) {
        logLine("[CAM] Sensor PID: 0x%04X\n", s->id.PID);
      }
      if (s && s->id.PID == OV2640_PID) {
        // OV2640 NoIR Einstellungen
        s->set_vflip(s, 1);            // V-Flip (gleiche Einbaulage wie vorher – ggf. anpassen)
        s->set_hmirror(s, 1);          // H-Mirror (ggf. anpassen)
        s->set_brightness(s, 0);       // Auto-Exposure regelt Helligkeit selbst
        s->set_contrast(s, 0);         // Standard
        s->set_saturation(s, 0);       // Standard (NoIR hat ohnehin verschobene Farbbalance)
        s->set_whitebal(s, 1);         // Auto-Weißabgleich ein
        s->set_awb_gain(s, 1);         // AWB-Gain ein
        s->set_wb_mode(s, 0);          // Auto-WB-Modus
        s->set_exposure_ctrl(s, 1);    // Auto-Belichtung ein
        s->set_aec2(s, 0);             // Night-Mode AEC aus: kürzere Belichtung, weniger Bewegungsunschärfe
        s->set_gain_ctrl(s, 1);        // Auto-Gain ein
        s->set_gainceiling(s, GAINCEILING_4X); // Gain-Limit: Kompromiss Helligkeit/Rauschen
        s->set_lenc(s, 1);             // Linsenkorrektur ein
        s->set_dcw(s, 1);              // Downsize-Filter ein (wichtig für VGA-Qualität)
        logLine("[CAM] OV2640 NoIR-Einstellungen angewendet\n");

        // Warmup: erste 5 Frames verwerfen (AE-Stabilisierung)
        logLine("[CAM] Warte auf AE-Stabilisierung (5 Frames × 300ms)...\n");
        for (int i = 0; i < 5; i++) {
          camera_fb_t *fb = esp_camera_fb_get();
          if (fb) esp_camera_fb_return(fb);
          delay(300);
        }
        logLine("[CAM] Kamera bereit\n");
      }
      return true;
    }
    logLine("[CAM] Init fehlgeschlagen (Versuch %d), Fehler: 0x%x\n", attempt, err);
    delay(1000);
  }

  logLine("[CAM] Init endgültig fehlgeschlagen\n");
  return false;
}
