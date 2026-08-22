#include "debug/ESP32P4_Debug.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

uint32_t ESP32P4_Debug::_mask = 0;
uint32_t ESP32P4_Debug::_sketch_mask = 0;
uint32_t ESP32P4_Debug::_period_ms = 2000;
uint32_t ESP32P4_Debug::_last_ms[10] = {};
char ESP32P4_Debug::_app[32] = "csi";
char ESP32P4_Debug::_stall[128] = "";
bool ESP32P4_Debug::_ok = false;

static int bitIndex(uint32_t bit) {
  for (int i = 0; i < 10; i++) {
    if (bit & (1u << i)) return i;
  }
  return 0;
}

const char *ESP32P4_Debug::bitName(uint32_t bit) {
  switch (bit) {
    case ESP32P4_DBG_CAM: return "cam";
    case ESP32P4_DBG_PPA: return "ppa";
    case ESP32P4_DBG_JPEG: return "jpeg";
    case ESP32P4_DBG_STREAM: return "stream";
    case ESP32P4_DBG_WIFI: return "wifi";
    case ESP32P4_DBG_AUDIO: return "audio";
    case ESP32P4_DBG_H264: return "h264";
    case ESP32P4_DBG_SD: return "sd";
    case ESP32P4_DBG_ISP: return "isp";
    case ESP32P4_DBG_NET: return "net";
    default: return "dbg";
  }
}

void ESP32P4_Debug::namesJson(char *out, size_t cap) {
  if (!out || !cap) return;
  snprintf(out, cap,
           "{\"cam\":1,\"ppa\":2,\"jpeg\":4,\"stream\":8,\"wifi\":16,\"audio\":32,"
           "\"h264\":64,\"sd\":128,\"isp\":256,\"net\":512,\"live\":%u}",
           (unsigned)ESP32P4_DBG_LIVE);
}

void ESP32P4_Debug::loadNvs(uint32_t sketch_mask) {
  Preferences p;
  if (!p.begin("csi_dbg", true)) {
    _mask = sketch_mask;
    return;
  }
  _mask = p.getUInt("mask", sketch_mask);
  _period_ms = p.getUInt("ms", 2000);
  if (_period_ms < 200) _period_ms = 200;
  if (_period_ms > 60000) _period_ms = 60000;
  p.end();
}

void ESP32P4_Debug::saveNvs() {
  Preferences p;
  if (!p.begin("csi_dbg", false)) return;
  p.putUInt("mask", _mask);
  p.putUInt("ms", _period_ms);
  p.end();
}

void ESP32P4_Debug::begin(const char *app, uint32_t sketch_mask) {
  if (app && app[0]) {
    strncpy(_app, app, sizeof(_app) - 1);
    _app[sizeof(_app) - 1] = '\0';
  }
  _sketch_mask = sketch_mask;
  loadNvs(sketch_mask);
  _ok = true;
  Serial.printf("CSI_DBG: app=%s mask=0x%x period=%ums (NVS csi_dbg; d / d=<mask> / d=r)\n", _app,
                (unsigned)_mask, (unsigned)_period_ms);
}

void ESP32P4_Debug::ensure() {
  if (_ok) return;
  begin("csi", 0);
}

void ESP32P4_Debug::setMask(uint32_t mask, bool persist) {
  _mask = mask & ESP32P4_DBG_ALL;
  if (persist) saveNvs();
  Serial.printf("CSI_DBG: mask=0x%x saved=%u\n", (unsigned)_mask, persist ? 1u : 0u);
}

void ESP32P4_Debug::setPeriodMs(uint32_t ms, bool persist) {
  if (ms < 200) ms = 200;
  if (ms > 60000) ms = 60000;
  _period_ms = ms;
  if (persist) saveNvs();
}

void ESP32P4_Debug::vlog(char tag, uint32_t bit, const char *fmt, va_list ap) {
  char line[192];
  vsnprintf(line, sizeof(line), fmt, ap);
  Serial.printf("CSI_%c %s %s %u %s\n", tag, _app, bitName(bit), (unsigned)millis(), line);
}

void ESP32P4_Debug::event(uint32_t bit, const char *fmt, ...) {
  if (!_mask || !(_mask & bit)) return;
  va_list ap;
  va_start(ap, fmt);
  vlog('E', bit, fmt, ap);
  va_end(ap);
}

void ESP32P4_Debug::periodic(uint32_t bit, const char *fmt, ...) {
  if (!_mask || !(_mask & bit)) return;
  const int i = bitIndex(bit);
  const uint32_t now = millis();
  if (_last_ms[i] && (now - _last_ms[i]) < _period_ms) return;
  _last_ms[i] = now;
  va_list ap;
  va_start(ap, fmt);
  vlog('D', bit, fmt, ap);
  va_end(ap);
}

void ESP32P4_Debug::stall(uint32_t bit, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(_stall, sizeof(_stall), fmt, ap);
  va_end(ap);
  if (!_mask || !(_mask & bit)) return;
  Serial.printf("CSI_S %s %s %u %s\n", _app, bitName(bit), (unsigned)millis(), _stall);
}

void ESP32P4_Debug::poll() {
  if (!_ok) return;
  if (!Serial.available()) return;
  const int c = Serial.peek();
  if (c != 'd' && c != 'D') return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (s == "d" || s == "D") {
    Serial.printf("CSI_DBG: app=%s mask=0x%x sketch=0x%x ms=%u stall=\"%s\"\n", _app,
                  (unsigned)_mask, (unsigned)_sketch_mask, (unsigned)_period_ms, _stall);
    return;
  }
  if (s.equalsIgnoreCase("d=r")) {
    setMask(_sketch_mask, true);
    return;
  }
  if (s.startsWith("d=") || s.startsWith("D=")) {
    uint32_t m = (uint32_t)strtoul(s.c_str() + 2, nullptr, 0);
    setMask(m, true);
  }
}
