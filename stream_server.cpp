// ============================================================
// Stream-Server – esp_http_server auf Port 81
//
// /stream : MJPEG-Livestream (hält Kamera wach solange verbunden)
// /jpg    : JPEG-Snapshot   (Liveness-Check für sopobihocam.html)
//
// Als .cpp-Datei (nicht .ino), damit der Arduino-Preprocessor
// keine falschen Auto-Prototypen für httpd_req_t generiert.
// ============================================================

#include <Arduino.h>
#include <WebServer.h>      // wird von config.h benötigt (extern WebServer server)
#include "config.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include <sys/socket.h>   // setsockopt / TCP_NODELAY

#define MJPEG_BOUNDARY      "frame"
#define MJPEG_CONTENT_TYPE  "multipart/x-mixed-replace;boundary=" MJPEG_BOUNDARY
#define MJPEG_BOUNDARY_LINE "\r\n--" MJPEG_BOUNDARY "\r\n"
#define MJPEG_PART_HDR      "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

extern unsigned long lastClientActivity;
extern volatile bool streamClientConnected;
extern uint32_t rtc_conn_count;

static httpd_handle_t stream_httpd = NULL;

static esp_err_t mjpeg_stream_handler(httpd_req_t *req) {
  camera_fb_t *fb  = NULL;
  uint8_t *jpg_buf = NULL;
  esp_err_t res    = ESP_OK;
  char part_buf[128];

  res = httpd_resp_set_type(req, MJPEG_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

  // TCP optimieren: Nagle deaktivieren (reduziert Latenz bei kleinen Chunks)
  int sock = httpd_req_to_sockfd(req);
  if (sock >= 0) {
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
  }

  // Client verbunden: Kamera bleibt wach bis dieser Handler endet
  streamClientConnected = true;
  lastClientActivity    = millis();
  rtc_conn_count++;
  logLine("[STREAM] Client verbunden (Verbindung #%u)\n", rtc_conn_count);

  int frameCount = 0;
  unsigned long fpsTimer = millis();
  int fpsCount = 0;

  while (res == ESP_OK) {

    unsigned long t0 = millis();

    fb = esp_camera_fb_get();
    if (!fb) {
      logLine("[STREAM] Frame-Fehler – Stream abgebrochen\n");
      res = ESP_FAIL;
      break;
    }

    unsigned long tCapture = millis() - t0;

    size_t      jpg_len;
    const char *send_buf;

    if (fb->format == PIXFORMAT_JPEG) {
      if (fb->len < 5000 ||
          fb->buf[0] != 0xFF || fb->buf[1] != 0xD8 ||
          fb->buf[fb->len - 2] != 0xFF || fb->buf[fb->len - 1] != 0xD9) {
        Serial.printf("[STREAM] Korrupter Frame übersprungen (len=%u)\n", fb->len);
        esp_camera_fb_return(fb);
        fb = NULL;
        continue;
      }
      jpg_len  = fb->len;
      send_buf = (const char *)fb->buf;
    } else {
      size_t jl = 0;
      bool ok = frame2jpg(fb, 80, &jpg_buf, &jl);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!ok || !jpg_buf) {
        jpg_buf = NULL;
        continue;
      }
      jpg_len  = jl;
      send_buf = (const char *)jpg_buf;
    }

    size_t hlen = snprintf(part_buf, sizeof(part_buf),
                           MJPEG_BOUNDARY_LINE MJPEG_PART_HDR,
                           (unsigned)jpg_len);
    unsigned long tSendStart = millis();
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, send_buf, jpg_len);
    }
    unsigned long tSend  = millis() - tSendStart;
    unsigned long tTotal = millis() - t0;

    if (fb)      { esp_camera_fb_return(fb); fb = NULL; }
    if (jpg_buf) { free(jpg_buf); jpg_buf = NULL; }

    // Adaptives Pacing: Ziel 10fps (100ms/Frame)
    {
      long delay_ms = 100L - (long)tTotal;
      if (tSend > 60L) delay_ms += (long)tSend;
      if (delay_ms <  20L) delay_ms =  20L;
      if (delay_ms > 500L) delay_ms = 500L;
      vTaskDelay(pdMS_TO_TICKS((uint32_t)delay_ms));
    }

    frameCount++;
    fpsCount++;

    if (frameCount <= 5) {
      Serial.printf("[STREAM] Frame %d: %u B, cap=%lums, send=%lums, total=%lums\n",
                    frameCount, jpg_len, tCapture, tSend, tTotal);
    }
    if (millis() - fpsTimer >= 5000) {
      Serial.printf("[STREAM] %.1f fps, send=%lums\n",
                    fpsCount * 1000.0f / (float)(millis() - fpsTimer), tSend);
      fpsTimer = millis();
      fpsCount = 0;
    }
  }

  httpd_resp_send_chunk(req, NULL, 0);

  // Client getrennt → Sleep-Timer läuft ab (wait_for_client_s)
  streamClientConnected = false;
  lastClientActivity    = millis();
  logLine("[STREAM] Client getrennt (Frames gesamt: %d)\n", frameCount);
  return res;
}

// GET /jpg – Einzelbild-Snapshot für Liveness-Check (sopobihocam.html)
// und Live-Vorschau in der Web-UI
static esp_err_t jpeg_snapshot_handler(httpd_req_t *req) {
  camera_fb_t *fb      = NULL;
  uint8_t     *jpg_buf = NULL;
  esp_err_t    res     = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  size_t      jpg_len;
  const char *send_buf;

  if (fb->format == PIXFORMAT_JPEG) {
    jpg_len  = fb->len;
    send_buf = (const char *)fb->buf;
  } else {
    size_t jl = 0;
    bool ok = frame2jpg(fb, 80, &jpg_buf, &jl);
    esp_camera_fb_return(fb);
    fb = NULL;
    if (!ok || !jpg_buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    jpg_len  = jl;
    send_buf = (const char *)jpg_buf;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  res = httpd_resp_send(req, send_buf, (ssize_t)jpg_len);

  if (fb)      esp_camera_fb_return(fb);
  if (jpg_buf) free(jpg_buf);

  return res;
}

void startStreamServer() {
  httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
  config.server_port       = 81;
  config.ctrl_port         = 32769;
  config.recv_wait_timeout = 30;
  config.send_wait_timeout = 30;
  config.max_open_sockets  = 4;   // /stream + /jpg gleichzeitig möglich
  config.lru_purge_enable  = true;

  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = mjpeg_stream_handler,
    .user_ctx = NULL
  };
  httpd_uri_t snapshot_uri = {
    .uri      = "/jpg",
    .method   = HTTP_GET,
    .handler  = jpeg_snapshot_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &snapshot_uri);
    logLine("[STREAM] Server auf Port 81 gestartet (/stream, /jpg)\n");
  } else {
    logLine("[STREAM] Fehler beim Starten des Stream-Servers\n");
  }
}
