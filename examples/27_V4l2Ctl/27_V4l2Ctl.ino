#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "27_V4l2Ctl"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#if __has_include("esp_cam_sensor_types.h")
#include "esp_cam_sensor_types.h"
#endif

// V4L2 POSIX + v4l2-ctl on top of camera_fb_t.
// cam.begin() / capture() stay the easy path. v4l.begin() adds /dev/video0.
// v4l2m.begin() adds M2M /dev/video10–12 and ISP meta /dev/video20.
// Type v4l2-ctl commands on Serial (115200), or use ioctl(fd, VIDIOC_*, ...).
// Do not also start Arduino ESP_Video on this CSI host.

ESP32P4_Camera cam;
ESP32P4_V4l2 v4l;
ESP32P4_Jpeg jpeg;
ESP32P4_H264 h264;
ESP32P4_V4l2M2m v4l2m;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 27_V4l2Ctl ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera begin FAILED");
    while (true) delay(1000);
  }
  if (!v4l.begin(&cam)) {
    Serial.println("V4L2 begin FAILED");
    while (true) delay(1000);
  }

  jpeg.begin(cam.width(), cam.height());
  h264.begin(cam.width(), cam.height());
  v4l2m.begin({&cam, &jpeg, &h264});

  v4l.ctl("--list-formats");
  v4l.ctl("--list-ctrls");

#if ESP32P4_HAS_V4L2
  int fd = v4l.fd();
  if (fd >= 0) {
    struct v4l2_capability cap = {};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
      Serial.printf("POSIX ioctl QUERYCAP driver=%s card=%s\n", cap.driver, cap.card);
    }
    struct v4l2_control c = {};
    c.id = V4L2_CID_GAIN;
    if (ioctl(fd, VIDIOC_G_CTRL, &c) == 0) Serial.printf("POSIX G_CTRL gain=%d\n", (int)c.value);
    struct v4l2_requestbuffers req = {};
    req.count = cam.fbCount();
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == 0) {
      struct v4l2_buffer qb = {};
      qb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      qb.memory = V4L2_MEMORY_MMAP;
      qb.index = 0;
      if (ioctl(fd, VIDIOC_QUERYBUF, &qb) == 0) {
        void *map = v4l.mmap(qb.length, (off_t)qb.m.offset);
        Serial.printf("V4L2 MMAP slot0 %p  %u bytes  (POSIX mmap ENOSYS - use v4l.mmap)\n", map,
                      (unsigned)qb.length);
      }
    }
#ifdef V4L2_CTRL_CLASS_ESP_CAM_IOCTL
#if __has_include("esp_cam_sensor_types.h")
    {
      esp_cam_sensor_id_t chip = {};
      struct v4l2_ext_control xc = {};
      xc.id = ESP_CAM_SENSOR_IOC_G_CHIP_ID;
      xc.size = sizeof(chip);
      xc.p_u8 = (uint8_t *)&chip;
      struct v4l2_ext_controls xcs = {};
      xcs.ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL;
      xcs.count = 1;
      xcs.controls = &xc;
      if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &xcs) == 0)
        Serial.printf("ESP_CAM_IOCTL G_CHIP_ID pid=0x%04x\n", chip.pid);
    }
#endif
#endif
  }
  if (v4l2m.fdJpegEnc() >= 0) {
    struct v4l2_capability cap = {};
    if (ioctl(v4l2m.fdJpegEnc(), VIDIOC_QUERYCAP, &cap) == 0) {
      Serial.printf("M2M JPEG enc %s caps=0x%x\n", ESP32P4_V4L2_DEV_JPEG_ENC, cap.capabilities);
    }
  }
  if (v4l2m.fdIspStats() >= 0) {
    Serial.printf("ISP stats node %s fd=%d\n", ESP32P4_V4L2_DEV_ISP, v4l2m.fdIspStats());
  }
#endif

  Serial.println("Serial: --list-ctrls | --set-ctrl gain=16 | --get-fmt-video | --stream-count=1");
  Serial.println("M2M: ioctl on /dev/video10 (JPEG), /dev/video11 (H264), /dev/video12 (dec), /dev/video20 (ISP)");
  Serial.println("Do not mix --stream-count with the capture() loop at the same time.");
}

void loop() {
  dbg.poll();
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) v4l.ctl(line.c_str());
    return;
  }
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) return;
  Serial.printf("fb %ux%u %u bytes  (easy path; V4L2 node is %s)\n", fb->width, fb->height,
                (unsigned)fb->len, v4l.path());
  cam.release(fb);
  delay(500);
}
