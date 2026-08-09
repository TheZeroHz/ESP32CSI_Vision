#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/sensors/esp32p4_sensor_ops_decl.h"

#include <Arduino.h>

// Probe order: popular boards first, then Espressif MIPI, then market, then stubs.
static const esp32p4_cam_sensor_ops_t *registry_at(size_t i) {
  static const esp32p4_cam_sensor_ops_t *list[] = {
      ov5647_sensor_ops(),
      imx708_sensor_ops(),
      sc2336_sensor_ops(),
      imx219_sensor_ops(),
      ov5645_sensor_ops(),
      ov9281_sensor_ops(),
      os02n10_sensor_ops(),
      sc035hgs_sensor_ops(),
      ov2710_sensor_ops(),
      sc202cs_sensor_ops(),
      sc1346_sensor_ops(),
      sc030iot_sensor_ops(),
      os04c10_sensor_ops(),
      sti2250_sensor_ops(),
      mira220_sensor_ops(),
      gc2145_sensor_ops(),
      sc121at_sensor_ops(),
      imx477_sensor_ops(),
      gc2083_sensor_ops(),
      gc2093_sensor_ops(),
      imx335_sensor_ops(),
      imx415_sensor_ops(),
      ov7251_sensor_ops(),
      imx296_sensor_ops(),
      imx462_sensor_ops(),
      arducam_imx500_sensor_ops(),
  };
  if (i >= sizeof(list) / sizeof(list[0])) return nullptr;
  return list[i];
}

const esp32p4_cam_sensor_ops_t *const *esp32p4_cam_sensor_registry(void) {
  static const esp32p4_cam_sensor_ops_t *table[32];
  static bool ready = false;
  if (!ready) {
    size_t n = 0;
    for (;; n++) {
      const esp32p4_cam_sensor_ops_t *ops = registry_at(n);
      if (!ops) break;
      table[n] = ops;
    }
    table[n] = nullptr;
    ready = true;
  }
  return table;
}

size_t esp32p4_cam_sensor_registry_count(void) {
  size_t n = 0;
  const esp32p4_cam_sensor_ops_t *const *t = esp32p4_cam_sensor_registry();
  while (t[n]) n++;
  return n;
}

const esp32p4_cam_sensor_ops_t *esp32p4_cam_sensor_find(esp32p4_cam_sensor_t id) {
  const esp32p4_cam_sensor_ops_t *const *t = esp32p4_cam_sensor_registry();
  for (size_t i = 0; t[i]; i++) {
    if (t[i]->id == id) return t[i];
  }
  return nullptr;
}

const esp32p4_cam_sensor_ops_t *esp32p4_cam_sensor_probe(esp32p4_cam_sensor_t prefer, uint8_t *addr7_out) {
  const esp32p4_cam_sensor_ops_t *const *t = esp32p4_cam_sensor_registry();
  if (prefer != ESP32P4_SENSOR_AUTO) {
    const esp32p4_cam_sensor_ops_t *ops = esp32p4_cam_sensor_find(prefer);
    if (ops && ops->detect && ops->detect(addr7_out)) return ops;
    Serial.printf("CSI: preferred %s not found — AUTO probe\n", ops ? ops->name : "?");
  }
  for (size_t i = 0; t[i]; i++) {
    uint8_t a = 0;
    if (!t[i]->detect) continue;
    if (t[i]->detect(&a)) {
      if (addr7_out) *addr7_out = a;
      Serial.printf("CSI: probed %s @ 0x%02X (%s)\n", t[i]->name, a,
                    t[i]->support == ESP32P4_CAM_SUPPORT_DETECT_ONLY ? "detect-only"
                    : t[i]->support == ESP32P4_CAM_SUPPORT_EXPERIMENTAL ? "experimental"
                                                                       : "full");
      return t[i];
    }
  }
  return nullptr;
}
