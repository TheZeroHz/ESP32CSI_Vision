#include "jpeg/ESP32P4_Jpeg.h"

#include <string.h>

#include "driver/jpeg_decode.h"
#include "driver/jpeg_encode.h"
#include "mem/ESP32P4_Psram.h"

bool ESP32P4_Jpeg::begin(uint16_t max_w, uint16_t max_h, uint8_t quality) {
  end();
  _quality = quality;
  _max_w = max_w;
  _max_h = max_h;

  jpeg_encode_engine_cfg_t eng = {};
  eng.intr_priority = 0;
  eng.timeout_ms = 80;
  jpeg_encoder_handle_t enc = nullptr;
  if (jpeg_new_encoder_engine(&eng, &enc) != ESP_OK) return false;
  _enc = enc;

  jpeg_decode_engine_cfg_t deng = {};
  deng.intr_priority = 0;
  deng.timeout_ms = 200;
  jpeg_decoder_handle_t dec = nullptr;
  if (jpeg_new_decoder_engine(&deng, &dec) != ESP_OK) {
    jpeg_del_encoder_engine(enc);
    _enc = nullptr;
    return false;
  }
  _dec = dec;

  jpeg_encode_memory_alloc_cfg_t in_cfg = {};
  in_cfg.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER;
  size_t got = 0;
  _in = (uint8_t *)jpeg_alloc_encoder_mem((size_t)max_w * max_h * 2, &in_cfg, &got);
  _in_cap = got;
  if (!_in) {
    _in = (uint8_t *)esp32p4_psram_alloc((size_t)max_w * max_h * 2);
    _in_cap = (size_t)max_w * max_h * 2;
  }
  return _in != nullptr;
}

void ESP32P4_Jpeg::end() {
  if (_enc) {
    jpeg_del_encoder_engine((jpeg_encoder_handle_t)_enc);
    _enc = nullptr;
  }
  if (_dec) {
    jpeg_del_decoder_engine((jpeg_decoder_handle_t)_dec);
    _dec = nullptr;
  }
  if (_in) {
    // jpeg_alloc buffers are not always heap_caps_free-safe; only free psram path if needed
    _in = nullptr;
    _in_cap = 0;
  }
}

size_t ESP32P4_Jpeg::encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap) {
  if (!fb) return 0;
  return encode(fb->buf, fb->width, fb->height, out, out_cap);
}

size_t ESP32P4_Jpeg::encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out,
                            size_t out_cap) {
  if (!_enc || !rgb565 || !out || !w || !h) return 0;
  size_t rgb_bytes = (size_t)w * h * 2;
  if (rgb_bytes > _in_cap) return 0;
  memcpy(_in, rgb565, rgb_bytes);

  jpeg_encode_cfg_t cfg = {};
  cfg.width = w;
  cfg.height = h;
  cfg.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
  cfg.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
  cfg.image_quality = _quality;
  uint32_t jlen = 0;
  if (jpeg_encoder_process((jpeg_encoder_handle_t)_enc, &cfg, _in, rgb_bytes, out, out_cap, &jlen) !=
      ESP_OK) {
    return 0;
  }
  if (jlen >= 2 && !(out[jlen - 2] == 0xFF && out[jlen - 1] == 0xD9) && jlen + 2 <= out_cap) {
    out[jlen++] = 0xFF;
    out[jlen++] = 0xD9;
  }
  return jlen;
}

bool ESP32P4_Jpeg::decodeInfo(const uint8_t *jpg, size_t jpg_len, uint32_t *w, uint32_t *h) {
  jpeg_decode_picture_info_t info = {};
  if (jpeg_decoder_get_info(jpg, jpg_len, &info) != ESP_OK) return false;
  if (w) *w = info.width;
  if (h) *h = info.height;
  return true;
}

size_t ESP32P4_Jpeg::decode(const uint8_t *jpg, size_t jpg_len, uint8_t *rgb_out, size_t out_cap,
                            uint32_t *w, uint32_t *h) {
  if (!_dec || !jpg || !rgb_out) return 0;
  jpeg_decode_cfg_t cfg = {};
  cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
  cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
  cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
  uint32_t out_size = 0;
  if (jpeg_decoder_process((jpeg_decoder_handle_t)_dec, &cfg, jpg, jpg_len, rgb_out, out_cap,
                           &out_size) != ESP_OK) {
    return 0;
  }
  uint32_t iw = 0, ih = 0;
  decodeInfo(jpg, jpg_len, &iw, &ih);
  if (w) *w = iw;
  if (h) *h = ih;
  return out_size;
}
