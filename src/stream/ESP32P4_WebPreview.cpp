#include "stream/ESP32P4_WebPreview.h"

#include "mem/ESP32P4_Psram.h"

#include <string.h>
#include "freertos/task.h"

static const char kIndex[] =
    "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>ESP32CSI</title>"
    "<style>body{font:14px/1.4 system-ui,sans-serif;margin:16px;background:#111;color:#eee}"
    "img{max-width:100%;background:#000}pre{background:#1b1b1b;padding:12px;overflow:auto;"
    "white-space:pre-wrap;border-radius:8px}a{color:#8ab4ff}</style>"
    "<h1 id=t>preview</h1>"
    "<p><img src='/stream' alt=live></p>"
    "<p>still <a href='/jpg'>/jpg</a> · json <a href='/dets'>/dets</a></p>"
    "<pre id=j>{}</pre>"
    "<script>"
    "document.getElementById('t').textContent=document.title;"
    "async function tick(){try{const r=await fetch('/dets');"
    "document.getElementById('j').textContent=await r.text()}catch(e){}}"
    "tick();setInterval(tick,400);"
    "</script>";

bool ESP32P4_WebPreview::begin(uint16_t port, uint8_t quality, uint16_t max_w, uint16_t max_h) {
  end();
  _port = port;
  if (!_jpeg.begin(max_w, max_h, quality)) return false;
  _jpg_cap = (size_t)max_w * max_h / 2 + 32768;
  if (_jpg_cap < 64 * 1024) _jpg_cap = 64 * 1024;
  for (int i = 0; i < 2; i++) {
    _jpg[i] = (uint8_t *)esp32p4_psram_alloc(_jpg_cap);
    if (!_jpg[i]) {
      end();
      return false;
    }
    _jpg_len[i] = 0;
  }
  _mu = xSemaphoreCreateMutex();
  if (!_mu) {
    end();
    return false;
  }
  _http = new WebServer(port);
  if (!_http) {
    end();
    return false;
  }
  _http->on("/", [this]() { handleRoot(); });
  _http->on("/jpg", [this]() { handleJpg(); });
  _http->on("/stream", [this]() { handleStream(); });
  _http->on("/dets", [this]() { handleDets(); });
  _http->begin();
  _http_run = true;
  if (xTaskCreate(httpThunk, "webprev", 6144, this, 4, &_http_task) != pdPASS) {
    _http_task = nullptr;
    Serial.println("APP: WebPreview HTTP task FAILED — using loop()");
  }
  Serial.printf("APP: WebPreview http://0.0.0.0:%u/  (/stream /jpg /dets)\n", (unsigned)port);
  return true;
}

void ESP32P4_WebPreview::end() {
  _http_run = false;
  if (_http_task) {
    vTaskDelay(pdMS_TO_TICKS(20));
    vTaskDelete(_http_task);
    _http_task = nullptr;
  }
  if (_http) {
    _http->stop();
    delete _http;
    _http = nullptr;
  }
  for (int i = 0; i < 2; i++) {
    esp32p4_psram_free(_jpg[i]);
    _jpg[i] = nullptr;
    _jpg_len[i] = 0;
  }
  if (_mu) {
    vSemaphoreDelete(_mu);
    _mu = nullptr;
  }
  _jpeg.end();
  _ready = -1;
  _enc = 0;
}

void ESP32P4_WebPreview::loop() {
  if (_http && !_http_task) _http->handleClient();
}

void ESP32P4_WebPreview::httpThunk(void *arg) {
  static_cast<ESP32P4_WebPreview *>(arg)->httpLoop();
}

void ESP32P4_WebPreview::httpLoop() {
  while (_http_run && _http) {
    _http->handleClient();
    vTaskDelay(1);
  }
}

void ESP32P4_WebPreview::setTitle(const char *title) {
  if (!title) return;
  strncpy(_title, title, sizeof(_title) - 1);
  _title[sizeof(_title) - 1] = 0;
}

void ESP32P4_WebPreview::setStatusJson(const char *json) {
  if (!json) json = "{}";
  strncpy(_json, json, sizeof(_json) - 1);
  _json[sizeof(_json) - 1] = 0;
}

bool ESP32P4_WebPreview::present(const camera_fb_t *fb) {
  if (!fb || !fb->buf || !_jpg[_enc]) return false;
  if (_mu) xSemaphoreTake(_mu, portMAX_DELAY);
  size_t n = 0;
  if (fb->format == ESP32P4_PIXFORMAT_JPEG) {
    if (fb->len > _jpg_cap) {
      if (_mu) xSemaphoreGive(_mu);
      return false;
    }
    memcpy(_jpg[_enc], fb->buf, fb->len);
    n = fb->len;
  } else {
    n = _jpeg.encode(fb, _jpg[_enc], _jpg_cap);
  }
  if (!n) {
    if (_mu) xSemaphoreGive(_mu);
    return false;
  }
  _jpg_len[_enc] = n;
  _ready = _enc;
  _enc ^= 1;
  _last_jpeg = (uint32_t)n;
  _presented++;
  if (_mu) xSemaphoreGive(_mu);
  return true;
}

bool ESP32P4_WebPreview::presentRgb565(const uint16_t *rgb565, int w, int h) {
  if (!rgb565 || w <= 0 || h <= 0 || !_jpg[_enc]) return false;
  if (_mu) xSemaphoreTake(_mu, portMAX_DELAY);
  size_t n = _jpeg.encode((const uint8_t *)rgb565, (uint16_t)w, (uint16_t)h, _jpg[_enc], _jpg_cap);
  if (!n) {
    if (_mu) xSemaphoreGive(_mu);
    return false;
  }
  _jpg_len[_enc] = n;
  _ready = _enc;
  _enc ^= 1;
  _last_jpeg = (uint32_t)n;
  _presented++;
  if (_mu) xSemaphoreGive(_mu);
  return true;
}

void ESP32P4_WebPreview::handleRoot() {
  _http->sendHeader("Cache-Control", "no-store");
  _http->send(200, "text/html; charset=utf-8", kIndex);
}

void ESP32P4_WebPreview::handleDets() {
  _http->sendHeader("Cache-Control", "no-store");
  _http->sendHeader("Access-Control-Allow-Origin", "*");
  _http->send(200, "application/json", _json);
}

void ESP32P4_WebPreview::handleJpg() {
  if (_mu) xSemaphoreTake(_mu, portMAX_DELAY);
  int idx = _ready;
  size_t n = (idx >= 0) ? _jpg_len[idx] : 0;
  if (idx < 0 || !n) {
    if (_mu) xSemaphoreGive(_mu);
    _http->send(503, "text/plain", "no frame yet — call present(fb)");
    return;
  }
  WiFiClient cl = _http->client();
  _http->sendHeader("Cache-Control", "no-store");
  _http->setContentLength(n);
  _http->send(200, "image/jpeg", "");
  cl.write(_jpg[idx], n);
  if (_mu) xSemaphoreGive(_mu);
}

void ESP32P4_WebPreview::handleStream() {
  WiFiClient client = _http->client();
  client.print(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
      "Cache-Control: no-store\r\n"
      "Connection: close\r\n\r\n");
  uint32_t last_seq = 0;
  while (client.connected()) {
    if (_mu) xSemaphoreTake(_mu, portMAX_DELAY);
    int idx = _ready;
    uint32_t seq = _presented;
    size_t n = (idx >= 0) ? _jpg_len[idx] : 0;
    if (idx >= 0 && n && seq != last_seq) {
      last_seq = seq;
      client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                    (unsigned)n);
      client.write(_jpg[idx], n);
      client.print("\r\n");
      if (_mu) xSemaphoreGive(_mu);
    } else {
      if (_mu) xSemaphoreGive(_mu);
      delay(10);
    }
    delay(0);
  }
}
