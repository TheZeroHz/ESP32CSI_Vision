#pragma once

/**
 * QR / barcode decode for ESP32-P4 CSI RGB565 frames.
 * Engine: Apache-2.0 zxing-cpp 2.3.0 (https://github.com/zxing-cpp/zxing-cpp)
 * Prefer scanGray() on a half-res PPA luma buffer so MJPEG is not starved.
 *
 * Format mask bits match ZXing::BarcodeFormat (see formatName / defaultFormats).
 */

#include <stddef.h>
#include <stdint.h>

#ifndef ESP32P4_QR_MAX_PAYLOAD
#define ESP32P4_QR_MAX_PAYLOAD 512
#endif

#ifndef ESP32P4_QR_MAX_CODES
#define ESP32P4_QR_MAX_CODES 4
#endif

struct esp32p4_qr_code_t {
  int corners[4][2];  // TL, TR, BR, BL
  char payload[ESP32P4_QR_MAX_PAYLOAD];
  int payload_len;
  int version;
  int ecc_level;
  int format;  // ZXing BarcodeFormat as int (QRCode, MicroQR, …)
};

class ESP32P4_Qr {
 public:
  ESP32P4_Qr() = default;
  ~ESP32P4_Qr() { end(); }

  /**
   * @param max_w/max_h  PSRAM gray working buffer capacity (usually stream size).
   * @param try_downscale  Use PPA half-res gray first when frame is large (faster).
   */
  bool begin(int max_w = 640, int max_h = 480, bool try_downscale = true);
  void end();
  bool ready() const { return _gray != nullptr; }

  /** ZXing BarcodeFormat bit mask used by the next scan (default = all supported). */
  void setFormats(uint32_t mask);
  uint32_t formats() const { return _formats; }
  static uint32_t defaultFormats();
  /** Human-readable symbology name (e.g. "QRCode", "Code128"). */
  static const char *formatName(int format);

  /** Decode RGB565 (PPA luma → zxing-cpp). Returns count written to out. */
  int scan(const uint16_t *rgb565, int w, int h, esp32p4_qr_code_t *out, int max_out);

  /**
   * Decode a prebuilt GRAY8 buffer (no RGB copy / PPA). scale_x/y map corners back
   * to the RGB frame (e.g. 2,2 when gray is half-res).
   */
  int scanGray(const uint8_t *gray, int w, int h, float scale_x, float scale_y,
               esp32p4_qr_code_t *out, int max_out);

  /** Draw corner boxes + truncated payload on RGB565. */
  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_qr_code_t *codes, int n);

  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }
  bool lastUsedPpa() const { return _last_ppa; }
  bool lastUsedDownscale() const { return _last_downscale; }

 private:
  bool ensureGray(int w, int h);
  bool fillGrayPpa(const uint16_t *rgb565, int w, int h);
  int decodeGray(const uint8_t *gray, int w, int h, float scale_x, float scale_y,
                 esp32p4_qr_code_t *out, int max_out);

  uint8_t *_gray = nullptr;
  size_t _gray_cap = 0;
  uint8_t *_gray_small = nullptr;
  size_t _gray_small_cap = 0;
  int _cap_w = 0;
  int _cap_h = 0;
  bool _try_downscale = true;
  uint32_t _formats = 0;
  int _last_ms = 0;
  int _last_n = 0;
  bool _last_ppa = false;
  bool _last_downscale = false;
};
