/**
 * 09_SdCard — microSD read/write via ESP32CSI_Vision
 *
 * Guition JC-ESP32P4-M3 onboard TF slot (SDMMC + LDO ch4).
 * Format card FAT32. Serial @ 115200.
 */

#include <ESP32CSI_Vision.h>

ESP32P4_Sd sd;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 09_SdCard ===");

  // Same board preset style as ESP32P4_Camera
  if (!sd.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("sd.begin FAILED");
    while (true) delay(1000);
  }

  Serial.printf("card %llu MB  used %llu / %llu bytes\n",
                (unsigned long long)(sd.cardSize() / (1024ULL * 1024ULL)),
                (unsigned long long)sd.usedBytes(), (unsigned long long)sd.totalBytes());

  sd.listDir("/", 1);

  // Write / append / read helpers
  if (!sd.writeFile("/hello.txt", "Hello from ESP32CSI_Vision\n")) {
    Serial.println("writeFile failed");
  }
  sd.appendFile("/hello.txt", "second line\n");

  char buf[128];
  size_t n = 0;
  if (sd.readFile("/hello.txt", buf, sizeof(buf), &n)) {
    Serial.printf("read %u bytes: %s", (unsigned)n, buf);
  }

  // Raw FS access when you need File streams
  File log = sd.fs().open("/log.txt", FILE_APPEND);
  if (log) {
    log.printf("boot ms=%lu\n", (unsigned long)millis());
    log.close();
  }

  sd.listDir("/", 0);
  Serial.println("SD test done.");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 5000) return;
  last = millis();

  char line[48];
  snprintf(line, sizeof(line), "heartbeat %lu\n", (unsigned long)millis());
  if (sd.appendFile("/heartbeat.txt", line)) {
    Serial.print(line);
  } else {
    Serial.println("heartbeat append failed");
  }
}
