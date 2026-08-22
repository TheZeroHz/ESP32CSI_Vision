#pragma once

/**
 * Component debug pipeline. Sketch sets a default mask; NVS "csi_dbg"
 * overrides it so you can change logging without recompiling.
 *
 *   #ifndef APP_DEBUG
 *   #define APP_DEBUG ESP32P4_DBG_LIVE
 *   #endif
 *   ESP32P4_Debug dbg;
 *   dbg.begin("31_WiFiLiveAvFiles", APP_DEBUG);
 *
 * Serial:
 *   d     dump mask + last stall
 *   d=15  set mask (bitfield) and save to NVS
 *   d=r   restore the sketch APP_DEBUG mask and save
 *
 * HTTP (MJPEG UI port): GET /debug   GET /control?var=debug&val=<mask>
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

enum esp32p4_dbg_bit_t : uint32_t {
  ESP32P4_DBG_CAM = 1u << 0,
  ESP32P4_DBG_PPA = 1u << 1,
  ESP32P4_DBG_JPEG = 1u << 2,
  ESP32P4_DBG_STREAM = 1u << 3,
  ESP32P4_DBG_WIFI = 1u << 4,
  ESP32P4_DBG_AUDIO = 1u << 5,
  ESP32P4_DBG_H264 = 1u << 6,
  ESP32P4_DBG_SD = 1u << 7,
  ESP32P4_DBG_ISP = 1u << 8,
  ESP32P4_DBG_NET = 1u << 9,
  ESP32P4_DBG_LIVE = ESP32P4_DBG_CAM | ESP32P4_DBG_PPA | ESP32P4_DBG_JPEG | ESP32P4_DBG_STREAM |
                     ESP32P4_DBG_WIFI | ESP32P4_DBG_NET,
  ESP32P4_DBG_ALL = 0x3FFu,
};

class ESP32P4_Debug {
 public:
  /** Load NVS (overrides sketch_mask when "mask" was saved). */
  static void begin(const char *app, uint32_t sketch_mask = 0);
  static void ensure();
  static void poll();

  static bool on(uint32_t bit) { return (_mask & bit) != 0; }
  static uint32_t mask() { return _mask; }
  static void setMask(uint32_t mask, bool persist = true);
  static void setPeriodMs(uint32_t ms, bool persist = true);
  static uint32_t periodMs() { return _period_ms; }
  static const char *app() { return _app; }
  static const char *lastStall() { return _stall; }

  static void event(uint32_t bit, const char *fmt, ...);
  static void periodic(uint32_t bit, const char *fmt, ...);
  static void stall(uint32_t bit, const char *fmt, ...);

  static const char *bitName(uint32_t bit);
  static void namesJson(char *out, size_t cap);

 private:
  static void loadNvs(uint32_t sketch_mask);
  static void saveNvs();
  static void vlog(char tag, uint32_t bit, const char *fmt, va_list ap);

  static uint32_t _mask;
  static uint32_t _sketch_mask;
  static uint32_t _period_ms;
  static uint32_t _last_ms[10];
  static char _app[32];
  static char _stall[128];
  static bool _ok;
};

#define CSI_EVT(bit, fmt, ...) ESP32P4_Debug::event((bit), (fmt), ##__VA_ARGS__)
#define CSI_DBG(bit, fmt, ...) ESP32P4_Debug::periodic((bit), (fmt), ##__VA_ARGS__)
#define CSI_STALL(bit, fmt, ...) ESP32P4_Debug::stall((bit), (fmt), ##__VA_ARGS__)
