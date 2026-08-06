#include "who/ESP32P4_Who.h"

bool ESP32P4_WhoPipeline::begin(ESP32P4_Camera *cam, uint8_t queue_len) {
  end();
  if (!cam) return false;
  _cam = cam;
  if (queue_len < 1) queue_len = 1;
  _q = xQueueCreate(queue_len, sizeof(esp32p4_who_fb_t));
  if (!_q) return false;
  _run = true;
  if (xTaskCreatePinnedToCore(taskThunk, "p4who", 4096, this, 5, &_task, 1) != pdPASS) {
    _run = false;
    vQueueDelete(_q);
    _q = nullptr;
    return false;
  }
  return true;
}

void ESP32P4_WhoPipeline::end() {
  _run = false;
  if (_task) {
    vTaskDelay(pdMS_TO_TICKS(20));
    vTaskDelete(_task);
    _task = nullptr;
  }
  if (_q) {
    vQueueDelete(_q);
    _q = nullptr;
  }
  _cam = nullptr;
}

void ESP32P4_WhoPipeline::onFrame(esp32p4_who_cb_t cb, void *ctx) {
  _cb = cb;
  _cb_ctx = ctx;
}

bool ESP32P4_WhoPipeline::waitFrame(esp32p4_who_fb_t *out, uint32_t timeout_ms) {
  if (!_q || !out) return false;
  return xQueueReceive(_q, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void ESP32P4_WhoPipeline::taskThunk(void *arg) { static_cast<ESP32P4_WhoPipeline *>(arg)->taskLoop(); }

void ESP32P4_WhoPipeline::taskLoop() {
  while (_run && _cam) {
    camera_fb_t *fb = _cam->capture(1000);
    if (!fb) continue;
    esp32p4_who_fb_t w{};
    w.buf = fb->buf;
    w.len = fb->len;
    w.width = fb->width;
    w.height = fb->height;
    w.timestamp_us = fb->timestamp_us;
    if (_cb) _cb(&w, _cb_ctx);
    if (_q) {
      // drop oldest if full
      esp32p4_who_fb_t dump;
      if (uxQueueSpacesAvailable(_q) == 0) xQueueReceive(_q, &dump, 0);
      xQueueSend(_q, &w, 0);
    }
    _cam->release(fb);
  }
  vTaskDelete(nullptr);
}
