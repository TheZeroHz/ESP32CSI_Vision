#include "qr/ESP32P4_Qr.h"

#include "cv/ESP32P4_Cv.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "ppa/ESP32P4_Ppa.h"

#include "qr/zxing/Version.h"
#include "qr/zxing/ReadBarcode.h"

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

using namespace ZXing;

#ifndef ESP32P4_QR_SOFT_BUDGET_MS
#define ESP32P4_QR_SOFT_BUDGET_MS 350
#endif

uint32_t ESP32P4_Qr::defaultFormats() {
  const BarcodeFormats all =
      BarcodeFormat::QRCode | BarcodeFormat::MicroQRCode | BarcodeFormat::RMQRCode |
      BarcodeFormat::Aztec | BarcodeFormat::PDF417 | BarcodeFormat::Code128 | BarcodeFormat::Code39 |
      BarcodeFormat::Code93 | BarcodeFormat::Codabar | BarcodeFormat::EAN8 | BarcodeFormat::EAN13 |
      BarcodeFormat::UPCA | BarcodeFormat::UPCE | BarcodeFormat::ITF | BarcodeFormat::DataBar |
      BarcodeFormat::DataBarExpanded | BarcodeFormat::DataBarLimited;
  uint32_t bits = 0;
  for (auto f : all) bits |= (uint32_t)f;
  return bits;
}

static BarcodeFormats formatsFromMask(uint32_t mask) {
  BarcodeFormats out;
  mask &= ESP32P4_Qr::defaultFormats();
  for (uint32_t bit = 0; bit < 32; bit++) {
    uint32_t flag = 1u << bit;
    if (mask & flag) out |= static_cast<BarcodeFormat>(flag);
  }
  if (out.empty()) {
    out = BarcodeFormat::QRCode | BarcodeFormat::MicroQRCode | BarcodeFormat::RMQRCode |
          BarcodeFormat::Aztec | BarcodeFormat::PDF417 | BarcodeFormat::Code128 |
          BarcodeFormat::Code39 | BarcodeFormat::Code93 | BarcodeFormat::Codabar |
          BarcodeFormat::EAN8 | BarcodeFormat::EAN13 | BarcodeFormat::UPCA | BarcodeFormat::UPCE |
          BarcodeFormat::ITF | BarcodeFormat::DataBar | BarcodeFormat::DataBarExpanded |
          BarcodeFormat::DataBarLimited;
  }
  return out;
}

const char *ESP32P4_Qr::formatName(int format) {
  switch ((BarcodeFormat)format) {
    case BarcodeFormat::Aztec:
      return "Aztec";
    case BarcodeFormat::Codabar:
      return "Codabar";
    case BarcodeFormat::Code39:
      return "Code39";
    case BarcodeFormat::Code93:
      return "Code93";
    case BarcodeFormat::Code128:
      return "Code128";
    case BarcodeFormat::DataBar:
      return "DataBar";
    case BarcodeFormat::DataBarExpanded:
      return "DataBarExpanded";
    case BarcodeFormat::DataBarLimited:
      return "DataBarLimited";
    case BarcodeFormat::EAN8:
      return "EAN-8";
    case BarcodeFormat::EAN13:
      return "EAN-13";
    case BarcodeFormat::ITF:
      return "ITF";
    case BarcodeFormat::PDF417:
      return "PDF417";
    case BarcodeFormat::QRCode:
      return "QRCode";
    case BarcodeFormat::MicroQRCode:
      return "MicroQR";
    case BarcodeFormat::RMQRCode:
      return "rMQR";
    case BarcodeFormat::UPCA:
      return "UPC-A";
    case BarcodeFormat::UPCE:
      return "UPC-E";
    case BarcodeFormat::DataMatrix:
      return "DataMatrix";
    default:
      return "Unknown";
  }
}

void ESP32P4_Qr::setFormats(uint32_t mask) {
  mask &= defaultFormats();
  if (!mask) mask = defaultFormats();
  _formats = mask;
}

bool ESP32P4_Qr::begin(int max_w, int max_h, bool try_downscale) {
  end();
  if (max_w < 64) max_w = 64;
  if (max_h < 64) max_h = 64;
  _try_downscale = try_downscale;
  _formats = defaultFormats();
  size_t need = (size_t)max_w * (size_t)max_h;
  _gray = (uint8_t *)esp32p4_psram_alloc(need);
  if (!_gray) return false;
  _gray_cap = need;
  if (_try_downscale) {
    size_t small = (size_t)((max_w + 1) / 2) * (size_t)((max_h + 1) / 2);
    _gray_small = (uint8_t *)esp32p4_psram_alloc(small);
    _gray_small_cap = _gray_small ? small : 0;
  }
  (void)ESP32P4_Ppa::cv().begin();
  _cap_w = max_w;
  _cap_h = max_h;
  return true;
}

void ESP32P4_Qr::end() {
  if (_gray) {
    esp32p4_psram_free(_gray);
    _gray = nullptr;
  }
  if (_gray_small) {
    esp32p4_psram_free(_gray_small);
    _gray_small = nullptr;
  }
  _gray_cap = _gray_small_cap = 0;
  _cap_w = _cap_h = 0;
  _last_ms = _last_n = 0;
  _last_ppa = _last_downscale = false;
  _formats = 0;
}

bool ESP32P4_Qr::ensureGray(int w, int h) {
  if (!_gray || w <= 0 || h <= 0) return false;
  size_t need = (size_t)w * (size_t)h;
  if (need <= _gray_cap) return true;
  esp32p4_psram_free(_gray);
  _gray = (uint8_t *)esp32p4_psram_alloc(need);
  _gray_cap = _gray ? need : 0;
  _cap_w = w;
  _cap_h = h;
  return _gray != nullptr;
}

bool ESP32P4_Qr::fillGrayPpa(const uint16_t *rgb565, int w, int h) {
  if (!ensureGray(w, h)) return false;
  _last_ppa = ESP32P4_Ppa::cv().rgb565ToGray(rgb565, w, h, _gray);
  if (_last_ppa) return true;
  if (ESP32P4_Cv::toGray(rgb565, w, h, _gray, nullptr)) return true;
  size_t n = (size_t)w * (size_t)h;
  for (size_t i = 0; i < n; i++) _gray[i] = ESP32P4_Img::luma565(rgb565[i]);
  return true;
}

static bool payloadPlausible(const std::string &text) {
  if (text.empty() || text.size() > 160) return false;
  int printable = 0;
  for (unsigned char c : text) {
    if (c >= 32 && c < 127) printable++;
  }
  return printable * 5 >= (int)text.size() * 4;
}

static int packCodes(const Barcodes &codes, float scale_x, float scale_y, esp32p4_qr_code_t *out,
                     int max_out) {
  int n = 0;
  for (const auto &b : codes) {
    if (!b.isValid() || n >= max_out) continue;
    const std::string text = b.text();
    if (!payloadPlausible(text)) continue;
    esp32p4_qr_code_t &dst = out[n];
    const auto &pos = b.position();
    for (int c = 0; c < 4; c++) {
      dst.corners[c][0] = (int)lroundf(pos[c].x * scale_x);
      dst.corners[c][1] = (int)lroundf(pos[c].y * scale_y);
    }
    dst.format = (int)b.format();
    dst.version = 0;
    dst.ecc_level = 0;
    dst.payload_len = (int)text.size();
    if (dst.payload_len >= ESP32P4_QR_MAX_PAYLOAD) dst.payload_len = ESP32P4_QR_MAX_PAYLOAD - 1;
    if (dst.payload_len < 0) dst.payload_len = 0;
    memcpy(dst.payload, text.data(), (size_t)dst.payload_len);
    dst.payload[dst.payload_len] = '\0';
    n++;
  }
  return n;
}

int ESP32P4_Qr::decodeGray(const uint8_t *gray, int w, int h, float scale_x, float scale_y,
                           esp32p4_qr_code_t *out, int max_out) {
  if (!gray || !out || max_out <= 0 || w < 16 || h < 16) return 0;

  ImageView iv(gray, w, h, ImageFormat::Lum);
  ReaderOptions opts;
  opts.setFormats(formatsFromMask(_formats ? _formats : defaultFormats()));
  opts.setTryHarder(false);
  opts.setTryRotate(true);
  opts.setTryInvert(false);
  opts.setTryDownscale(false);
  opts.setMaxNumberOfSymbols(1);
  opts.setBinarizer(Binarizer::LocalAverage);
  opts.setReturnCodabarStartEnd(false);

  Barcodes codes;
  try {
    codes = ReadBarcodes(iv, opts);
  } catch (...) {
    return 0;
  }
  return packCodes(codes, scale_x, scale_y, out, max_out);
}

int ESP32P4_Qr::scanGray(const uint8_t *gray, int w, int h, float scale_x, float scale_y,
                         esp32p4_qr_code_t *out, int max_out) {
  _last_n = 0;
  _last_downscale = (scale_x > 1.01f || scale_y > 1.01f);
  _last_ppa = false;
  if (!gray || !out || max_out <= 0) return 0;
  const uint32_t t0 = millis();
  int n = decodeGray(gray, w, h, scale_x, scale_y, out, max_out);
  _last_n = n;
  _last_ms = (int)(millis() - t0);
  return n;
}

int ESP32P4_Qr::scan(const uint16_t *rgb565, int w, int h, esp32p4_qr_code_t *out, int max_out) {
  _last_n = 0;
  _last_downscale = false;
  if (!rgb565 || !out || max_out <= 0 || !ready()) return 0;

  const uint32_t t0 = millis();

  if (_try_downscale && _gray_small && w >= 640 && h >= 360) {
    int dw = w / 2;
    int dh = h / 2;
    size_t need = (size_t)dw * (size_t)dh;
    if (need <= _gray_small_cap &&
        ESP32P4_Ppa::cv().rgb565ToGrayScale(rgb565, w, h, _gray_small, dw, dh)) {
      _last_ppa = true;
      _last_downscale = true;
      int n = decodeGray(_gray_small, dw, dh, 2.0f, 2.0f, out, max_out);
      _last_n = n;
      _last_ms = (int)(millis() - t0);
      return n;
    }
  }

  if ((millis() - t0) >= (uint32_t)ESP32P4_QR_SOFT_BUDGET_MS) {
    _last_ms = (int)(millis() - t0);
    return 0;
  }

  if (!fillGrayPpa(rgb565, w, h)) {
    _last_ms = (int)(millis() - t0);
    return 0;
  }

  int n = decodeGray(_gray, w, h, 1.0f, 1.0f, out, max_out);
  _last_n = n;
  _last_ms = (int)(millis() - t0);
  return n;
}

void ESP32P4_Qr::draw(uint16_t *rgb565, int w, int h, const esp32p4_qr_code_t *codes, int n) {
  if (!rgb565 || !codes || n <= 0 || w < 8 || h < 8) return;
  const uint16_t col = 0x07E0;
  const uint16_t ink = 0xFFFF;
  for (int i = 0; i < n; i++) {
    const esp32p4_qr_code_t &c = codes[i];
    for (int k = 0; k < 4; k++) {
      int x0 = c.corners[k][0];
      int y0 = c.corners[k][1];
      int x1 = c.corners[(k + 1) & 3][0];
      int y1 = c.corners[(k + 1) & 3][1];
      ESP32P4_Cv::line(rgb565, w, h, x0, y0, x1, y1, col, 2);
    }
    int tx = c.corners[0][0];
    int ty = c.corners[0][1] - 12;
    if (ty < 2) ty = c.corners[0][1] + 4;
    if (tx < 2) tx = 2;
    char label[48];
    snprintf(label, sizeof(label), "%s: %.20s", formatName(c.format), c.payload);
    ESP32P4_Cv::putText(rgb565, w, h, tx, ty, label, ink, 1);
  }
}
