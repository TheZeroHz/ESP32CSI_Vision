#include "vision/ESP32P4_VisionAi.h"

#include "cv/ESP32P4_Cv.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool ESP32P4_VisionAi::letterboxRgb565(const uint16_t *src, int src_w, int src_h, uint8_t *dst,
                                       int model_w, int model_h, esp32p4_letterbox_t *meta,
                                       uint8_t pad_r, uint8_t pad_g, uint8_t pad_b) {
  if (!src || !dst || !meta || src_w <= 0 || src_h <= 0 || model_w <= 0 || model_h <= 0) return false;

  float scale = (float)model_w / (float)src_w;
  float scale_h = (float)model_h / (float)src_h;
  if (scale_h < scale) scale = scale_h;

  int new_w = (int)(src_w * scale + 0.5f);
  int new_h = (int)(src_h * scale + 0.5f);
  if (new_w < 1) new_w = 1;
  if (new_h < 1) new_h = 1;
  int pad_x = (model_w - new_w) / 2;
  int pad_y = (model_h - new_h) / 2;

  meta->scale = scale;
  meta->pad_x = pad_x;
  meta->pad_y = pad_y;
  meta->model_w = model_w;
  meta->model_h = model_h;
  meta->src_w = src_w;
  meta->src_h = src_h;

  // Fill pad
  size_t px = (size_t)model_w * (size_t)model_h;
  for (size_t i = 0; i < px; i++) {
    dst[i * 3 + 0] = pad_r;
    dst[i * 3 + 1] = pad_g;
    dst[i * 3 + 2] = pad_b;
  }

  // Nearest-neighbor scale into center
  for (int y = 0; y < new_h; y++) {
    int sy = (int)(y / scale);
    if (sy >= src_h) sy = src_h - 1;
    for (int x = 0; x < new_w; x++) {
      int sx = (int)(x / scale);
      if (sx >= src_w) sx = src_w - 1;
      uint16_t p = src[sy * src_w + sx];
      int dx = pad_x + x;
      int dy = pad_y + y;
      uint8_t *o = dst + ((size_t)dy * model_w + dx) * 3;
      o[0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
      o[1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
      o[2] = (uint8_t)((p & 0x1F) * 255 / 31);
    }
  }
  return true;
}

void ESP32P4_VisionAi::mapBoxToSrc(const esp32p4_letterbox_t &lb, float mx, float my, float mw,
                                   float mh, esp32p4_det_t *out) {
  if (!out || lb.scale <= 0.f) return;
  float x = (mx - (float)lb.pad_x) / lb.scale;
  float y = (my - (float)lb.pad_y) / lb.scale;
  float w = mw / lb.scale;
  float h = mh / lb.scale;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > lb.src_w) w = (float)lb.src_w - x;
  if (y + h > lb.src_h) h = (float)lb.src_h - y;
  out->x = (int)(x + 0.5f);
  out->y = (int)(y + 0.5f);
  out->w = (int)(w + 0.5f);
  out->h = (int)(h + 0.5f);
  if (out->w < 0) out->w = 0;
  if (out->h < 0) out->h = 0;
}

void ESP32P4_VisionAi::mapPointToSrc(const esp32p4_letterbox_t &lb, float mx, float my, float *sx,
                                     float *sy) {
  if (!sx || !sy || lb.scale <= 0.f) return;
  *sx = (mx - (float)lb.pad_x) / lb.scale;
  *sy = (my - (float)lb.pad_y) / lb.scale;
}

float ESP32P4_VisionAi::iou(const esp32p4_det_t &a, const esp32p4_det_t &b) {
  int x0 = a.x > b.x ? a.x : b.x;
  int y0 = a.y > b.y ? a.y : b.y;
  int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
  int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
  int iw = x1 - x0;
  int ih = y1 - y0;
  if (iw <= 0 || ih <= 0) return 0.f;
  float inter = (float)iw * (float)ih;
  float uni = (float)a.w * (float)a.h + (float)b.w * (float)b.h - inter;
  return uni > 0.f ? inter / uni : 0.f;
}

int ESP32P4_VisionAi::nms(const esp32p4_det_t *dets, int n, float iou_thr, int *keep_idx,
                          int max_keep) {
  if (!dets || !keep_idx || n <= 0 || max_keep <= 0) return 0;

  // Simple index sort by score desc (insertion — n is small after decode)
  int idx[256];
  int m = n < 256 ? n : 256;
  for (int i = 0; i < m; i++) idx[i] = i;
  for (int i = 1; i < m; i++) {
    int v = idx[i], j = i;
    while (j > 0 && dets[idx[j - 1]].score < dets[v].score) {
      idx[j] = idx[j - 1];
      j--;
    }
    idx[j] = v;
  }

  bool dead[256] = {};
  int kept = 0;
  for (int i = 0; i < m && kept < max_keep; i++) {
    int a = idx[i];
    if (dead[a]) continue;
    keep_idx[kept++] = a;
    for (int j = i + 1; j < m; j++) {
      int b = idx[j];
      if (dead[b]) continue;
      if (dets[a].category != dets[b].category) continue;
      if (iou(dets[a], dets[b]) > iou_thr) dead[b] = true;
    }
  }
  return kept;
}

void ESP32P4_VisionAi::softmax(float *logits, int n) {
  if (!logits || n <= 0) return;
  float mx = logits[0];
  for (int i = 1; i < n; i++)
    if (logits[i] > mx) mx = logits[i];
  float sum = 0.f;
  for (int i = 0; i < n; i++) {
    logits[i] = expf(logits[i] - mx);
    sum += logits[i];
  }
  if (sum <= 0.f) return;
  for (int i = 0; i < n; i++) logits[i] /= sum;
}

void ESP32P4_VisionAi::drawDets(uint16_t *img, int w, int h, const esp32p4_det_t *dets, int n,
                                uint16_t color, int thickness) {
  if (!img || !dets) return;
  for (int i = 0; i < n; i++) {
    esp32p4_rect_t r{dets[i].x, dets[i].y, dets[i].w, dets[i].h};
    ESP32P4_Img::fillRect565(img, w, h, r, color, thickness);
    char buf[40];
    const char *lab = dets[i].label && dets[i].label[0] ? dets[i].label : nullptr;
    if (lab)
      snprintf(buf, sizeof(buf), "%s %.2f", lab, (double)dets[i].score);
    else
      snprintf(buf, sizeof(buf), "%d %.2f", dets[i].category, (double)dets[i].score);
    ESP32P4_Cv::putText(img, w, h, dets[i].x, dets[i].y > 10 ? dets[i].y - 10 : dets[i].y + 2,
                        buf, color, 1);
  }
}

static void jsonEsc(char *dst, size_t cap, const char *s) {
  if (!dst || cap < 3) return;
  size_t o = 0;
  dst[o++] = '"';
  if (s) {
    for (; *s && o + 2 < cap; ++s) {
      char c = *s;
      if (c == '"' || c == '\\') {
        if (o + 3 >= cap) break;
        dst[o++] = '\\';
      }
      if ((unsigned char)c < 32) continue;
      dst[o++] = c;
    }
  }
  if (o + 1 < cap) dst[o++] = '"';
  dst[o] = '\0';
}

size_t ESP32P4_VisionAi::detsToJson(const esp32p4_det_t *dets, int n, char *buf, size_t cap, int ms) {
  if (!buf || cap < 16) return 0;
  if (n < 0) n = 0;
  size_t o = 0;
  auto app = [&](const char *s) {
    if (!s) return;
    size_t k = strlen(s);
    if (o + k >= cap) k = cap - 1 - o;
    if (k) {
      memcpy(buf + o, s, k);
      o += k;
    }
  };
  char tmp[96];
  int nw = snprintf(tmp, sizeof(tmp), "{\"n\":%d", n);
  (void)nw;
  app(tmp);
  if (ms >= 0) {
    snprintf(tmp, sizeof(tmp), ",\"ms\":%d", ms);
    app(tmp);
  }
  app(",\"dets\":[");
  for (int i = 0; i < n && dets; i++) {
    if (i) app(",");
    char lab[64];
    jsonEsc(lab, sizeof(lab), dets[i].label ? dets[i].label : "");
    snprintf(tmp, sizeof(tmp),
             "{\"label\":%s,\"class\":%d,\"score\":%.3f,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}", lab,
             dets[i].category, (double)dets[i].score, dets[i].x, dets[i].y, dets[i].w, dets[i].h);
    app(tmp);
  }
  app("]}");
  if (o < cap) buf[o] = '\0';
  else buf[cap - 1] = '\0';
  return o < cap ? o : cap - 1;
}

size_t ESP32P4_VisionAi::detsToLine(const esp32p4_det_t *dets, int n, char *buf, size_t cap) {
  if (!buf || cap < 2) return 0;
  buf[0] = '\0';
  if (!dets || n <= 0) return 0;
  size_t o = 0;
  for (int i = 0; i < n; i++) {
    char one[96];
    const char *lab = (dets[i].label && dets[i].label[0]) ? dets[i].label : "?";
    snprintf(one, sizeof(one), "%s%s %.2f @ %d,%d %dx%d", i ? " · " : "", lab, (double)dets[i].score,
             dets[i].x, dets[i].y, dets[i].w, dets[i].h);
    size_t k = strlen(one);
    if (o + k >= cap) break;
    memcpy(buf + o, one, k);
    o += k;
    buf[o] = '\0';
  }
  return o;
}

size_t ESP32P4_VisionAi::clsToJson(const esp32p4_cls_t *hits, int n, char *buf, size_t cap, int ms) {
  if (!buf || cap < 16) return 0;
  if (n < 0) n = 0;
  size_t o = 0;
  auto app = [&](const char *s) {
    size_t k = s ? strlen(s) : 0;
    if (o + k >= cap) k = cap - 1 - o;
    if (k) {
      memcpy(buf + o, s, k);
      o += k;
    }
  };
  char tmp[96];
  snprintf(tmp, sizeof(tmp), "{\"n\":%d", n);
  app(tmp);
  if (ms >= 0) {
    snprintf(tmp, sizeof(tmp), ",\"ms\":%d", ms);
    app(tmp);
  }
  app(",\"cls\":[");
  for (int i = 0; i < n && hits; i++) {
    if (i) app(",");
    char lab[64];
    jsonEsc(lab, sizeof(lab), hits[i].label ? hits[i].label : "");
    snprintf(tmp, sizeof(tmp), "{\"label\":%s,\"score\":%.3f}", lab, (double)hits[i].score);
    app(tmp);
  }
  app("]}");
  if (o < cap) buf[o] = '\0';
  else buf[cap - 1] = '\0';
  return o < cap ? o : cap - 1;
}

size_t ESP32P4_VisionAi::ocrToJson(const esp32p4_ocr_t *hits, int n, char *buf, size_t cap, int ms) {
  if (!buf || cap < 16) return 0;
  if (n < 0) n = 0;
  size_t o = 0;
  auto app = [&](const char *s) {
    size_t k = s ? strlen(s) : 0;
    if (o + k >= cap) k = cap - 1 - o;
    if (k) {
      memcpy(buf + o, s, k);
      o += k;
    }
  };
  char tmp[160];
  snprintf(tmp, sizeof(tmp), "{\"n\":%d", n);
  app(tmp);
  if (ms >= 0) {
    snprintf(tmp, sizeof(tmp), ",\"ms\":%d", ms);
    app(tmp);
  }
  app(",\"ocr\":[");
  for (int i = 0; i < n && hits; i++) {
    if (i) app(",");
    char lab[128];
    jsonEsc(lab, sizeof(lab), hits[i].text);
    snprintf(tmp, sizeof(tmp),
             "{\"text\":%s,\"score\":%.3f,\"quad\":[%d,%d,%d,%d,%d,%d,%d,%d]}", lab,
             (double)hits[i].score, hits[i].points[0], hits[i].points[1], hits[i].points[2],
             hits[i].points[3], hits[i].points[4], hits[i].points[5], hits[i].points[6],
             hits[i].points[7]);
    app(tmp);
  }
  app("]}");
  if (o < cap) buf[o] = '\0';
  else buf[cap - 1] = '\0';
  return o < cap ? o : cap - 1;
}

size_t ESP32P4_VisionAi::gestureToJson(const esp32p4_gesture_t *hits, int n, char *buf, size_t cap,
                                       int ms) {
  if (!buf || cap < 16) return 0;
  if (n < 0) n = 0;
  size_t o = 0;
  auto app = [&](const char *s) {
    size_t k = s ? strlen(s) : 0;
    if (o + k >= cap) k = cap - 1 - o;
    if (k) {
      memcpy(buf + o, s, k);
      o += k;
    }
  };
  char tmp[160];
  snprintf(tmp, sizeof(tmp), "{\"n\":%d", n);
  app(tmp);
  if (ms >= 0) {
    snprintf(tmp, sizeof(tmp), ",\"ms\":%d", ms);
    app(tmp);
  }
  app(",\"gesture\":[");
  for (int i = 0; i < n && hits; i++) {
    if (i) app(",");
    char lab[64];
    jsonEsc(lab, sizeof(lab), hits[i].label ? hits[i].label : "");
    snprintf(tmp, sizeof(tmp),
             "{\"label\":%s,\"score\":%.3f,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}", lab,
             (double)hits[i].score, hits[i].hand.x, hits[i].hand.y, hits[i].hand.w, hits[i].hand.h);
    app(tmp);
  }
  app("]}");
  if (o < cap) buf[o] = '\0';
  else buf[cap - 1] = '\0';
  return o < cap ? o : cap - 1;
}
