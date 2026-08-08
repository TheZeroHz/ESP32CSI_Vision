/**
 * Guition JC-ESP32P4-M3 onboard ES8311 mic (I2C + I2S pin map).
 * Adapted from esp32-ai mictest for ESP32CSI_Vision examples.
 */
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>

// Guition JC-ESP32P4-M3 codec / I2S pins
#ifndef ESP32P4_M3_I2C_SDA
#define ESP32P4_M3_I2C_SDA 7
#endif
#ifndef ESP32P4_M3_I2C_SCL
#define ESP32P4_M3_I2C_SCL 8
#endif
#ifndef ESP32P4_M3_ES8311_ADDR
#define ESP32P4_M3_ES8311_ADDR 0x18
#endif
#ifndef ESP32P4_M3_I2S_MCLK
#define ESP32P4_M3_I2S_MCLK 13
#endif
#ifndef ESP32P4_M3_I2S_BCLK
#define ESP32P4_M3_I2S_BCLK 12
#endif
#ifndef ESP32P4_M3_I2S_WS
#define ESP32P4_M3_I2S_WS 10
#endif
#ifndef ESP32P4_M3_I2S_DOUT
#define ESP32P4_M3_I2S_DOUT 9
#endif
#ifndef ESP32P4_M3_I2S_DIN
#define ESP32P4_M3_I2S_DIN 48
#endif
#ifndef ESP32P4_M3_ES8311_PA
#define ESP32P4_M3_ES8311_PA 11
#endif

static inline bool es8311_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ESP32P4_M3_ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static inline bool es8311_read(uint8_t reg, uint8_t *val) {
  Wire.beginTransmission(ESP32P4_M3_ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ESP32P4_M3_ES8311_ADDR, 1) != 1) return false;
  *val = (uint8_t)Wire.read();
  return true;
}

/** Power amp pin LOW = speaker off (less howl into the mic while recording). */
static inline void es8311_pa_off() {
  pinMode(ESP32P4_M3_ES8311_PA, OUTPUT);
  digitalWrite(ESP32P4_M3_ES8311_PA, LOW);
}

static inline bool es8311_init_mic() {
  uint8_t id = 0;
  if (!es8311_read(0xFD, &id)) {
    Serial.println("ES8311 not responding on I2C 0x18");
    return false;
  }
  Serial.printf("ES8311 id_high=0x%02X\n", id);

  if (!es8311_write(0x00, 0x1F)) return false;
  delay(20);
  if (!es8311_write(0x00, 0x00)) return false;
  if (!es8311_write(0x00, 0x80)) return false;

  if (!es8311_write(0x01, 0x3F)) return false;
  if (!es8311_write(0x02, ((1 - 1) << 5) | (0 << 3))) return false;
  if (!es8311_write(0x03, (0 << 6) | 0x10)) return false;
  if (!es8311_write(0x04, 0x10)) return false;
  if (!es8311_write(0x05, ((1 - 1) << 4) | (1 - 1))) return false;
  if (!es8311_write(0x06, (4 - 1))) return false;
  if (!es8311_write(0x07, 0x00)) return false;
  if (!es8311_write(0x08, 0xFF)) return false;

  uint8_t r00;
  if (!es8311_read(0x00, &r00)) return false;
  r00 &= 0xBF;
  if (!es8311_write(0x00, r00)) return false;
  if (!es8311_write(0x09, (3 << 2))) return false;
  if (!es8311_write(0x0A, (3 << 2))) return false;

  if (!es8311_write(0x0D, 0x01)) return false;
  if (!es8311_write(0x0E, 0x02)) return false;
  if (!es8311_write(0x12, 0x00)) return false;
  if (!es8311_write(0x13, 0x10)) return false;
  if (!es8311_write(0x1C, 0x6A)) return false;
  if (!es8311_write(0x37, 0x08)) return false;

  // ADC / mic path
  if (!es8311_write(0x14, 0x1A)) return false;
  if (!es8311_write(0x17, 0xC8)) return false;
  if (!es8311_write(0x16, 0x03)) return false;
  return true;
}

static inline bool es8311_i2s_begin(I2SClass &i2s, int sample_rate) {
  Wire.begin(ESP32P4_M3_I2C_SDA, ESP32P4_M3_I2C_SCL);
  Wire.setClock(100000);
  es8311_pa_off();

  i2s.setPins(ESP32P4_M3_I2S_BCLK, ESP32P4_M3_I2S_WS, ESP32P4_M3_I2S_DOUT,
              ESP32P4_M3_I2S_DIN, ESP32P4_M3_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, sample_rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO,
                 I2S_STD_SLOT_LEFT)) {
    Serial.println("I2S begin failed");
    return false;
  }
  if (!es8311_init_mic()) {
    Serial.println("ES8311 init failed");
    return false;
  }
  Serial.println("ES8311 mic + I2S OK");
  return true;
}
