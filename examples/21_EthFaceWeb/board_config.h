#pragma once

/**
 * board_config.h  —  sits NEXT TO this example (not in the library src/).
 *
 * Edit THIS file for YOUR ESP32-P4 carrier. Arduino IDE: it is a tab in the sketch.
 * Guide: docs/Custom-Boards.md
 *
 * Uncomment ONE value in each “pick one” block. Pins: copy from your schematic.
 */

#include <ESP32CSI_Vision.h>
#include <WiFi.h>
#include <Wire.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. BOARD LABEL (Serial only — GPIOs below always win)
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef ESP32CSI_BOARD
#define ESP32CSI_BOARD ESP32P4_BOARD_CUSTOM
// #define ESP32CSI_BOARD ESP32P4_BOARD_GUITION_M3
// #define ESP32CSI_BOARD ESP32P4_BOARD_WAVESHARE_NANO
// #define ESP32CSI_BOARD ESP32P4_BOARD_FUNCTION_EV
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. CAMERA — bus
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_CAM_BUS
#define CFG_CAM_BUS ESP32P4_CAM_BUS_CSI
// #define CFG_CAM_BUS ESP32P4_CAM_BUS_CSI
// #define CFG_CAM_BUS ESP32P4_CAM_BUS_DVP
// #define CFG_CAM_BUS ESP32P4_CAM_BUS_SPI
// #define CFG_CAM_BUS ESP32P4_CAM_BUS_UVC_HOST
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. CAMERA — sensor (MIPI CSI unless DVP/SPI/UVC bus above)
 *    AUTO probes the registry. Pick a chip to skip probe / force a driver.
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_CAM_SENSOR
#define CFG_CAM_SENSOR ESP32P4_SENSOR_AUTO
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_AUTO   /* SCCB probe */
/* ── Espressif MIPI ── */
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC2336
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV5647
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV5645
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV2710
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV9281
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC202CS
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC1346
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC030IOT
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC035HGS
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OS02N10
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OS04C10
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_GC2145
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_STI2250
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC121AT
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_MIRA220
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SC2331
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_GC2607
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV5640
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_LT6911
/* ── Sony / Pi-class MIPI ── */
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX708
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX219
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX477
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX462
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX335   // detect-only
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX415   // detect-only
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_IMX296   // detect-only
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_GC2083
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_GC2093
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV7251
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_ARDUCAM_IMX500
/* ── Other buses ── */
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_OV2640   // DVP
// #define CFG_CAM_SENSOR ESP32P4_SENSOR_SP0A39   // SPI
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. CAMERA — framesize (AUTO = driver default)
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_CAM_FRAMESIZE
#define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_AUTO
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_VGA          // 640x480
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_800X640
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_SVGA         // 800x600
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_HD           // 1280x720
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_SXGA         // 1280x960
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_1080P        // 1920x1080
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_2304X1296
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_QXGA
// #define CFG_CAM_FRAMESIZE ESP32P4_FRAMESIZE_5MP
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. CAMERA — framebuffer format after ISP
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_CAM_FORMAT
#define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_RGB565
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_RGB565
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_RGB888
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_YUV422   /* UYVY */
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_YUV420
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_YUYV
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_GRAY8
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_JPEG
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_RAW8
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_RAW10
// #define CFG_CAM_FORMAT ESP32P4_PIXFORMAT_RAW12
#endif

#ifndef CFG_CAM_FB_COUNT
#define CFG_CAM_FB_COUNT 3
#endif
#ifndef CFG_CAM_LANE_MBPS
#define CFG_CAM_LANE_MBPS 0 /* 0 = auto */
#endif
#ifndef CFG_CAM_CSI_ID
#define CFG_CAM_CSI_ID 0 /* P4 has one CSI host */
#endif

/* ── CSI SCCB / clocks / LDO ── */
#ifndef CFG_CAM_SDA
#define CFG_CAM_SDA 7
#endif
#ifndef CFG_CAM_SCL
#define CFG_CAM_SCL 8
#endif
#ifndef CFG_CAM_XCLK
#define CFG_CAM_XCLK -1 /* -1 = module crystal */
#endif
#ifndef CFG_CAM_XCLK_HZ
#define CFG_CAM_XCLK_HZ 24000000
#endif
#ifndef CFG_CAM_PWDN
#define CFG_CAM_PWDN -1
#endif
#ifndef CFG_CAM_RESET
#define CFG_CAM_RESET -1
#endif
#ifndef CFG_CAM_LDO_CHAN
#define CFG_CAM_LDO_CHAN 3
#endif
#ifndef CFG_CAM_LDO_MV
#define CFG_CAM_LDO_MV 2500
#endif
#ifndef CFG_CAM_I2C_ADDR
#define CFG_CAM_I2C_ADDR 0 /* 0 = probe */
#endif
#ifndef CFG_CAM_TEST_PATTERN
#define CFG_CAM_TEST_PATTERN 0
#endif
/* I2C instance: leave undefined for Wire. For Wire1 uncomment: */
/* #define CFG_CAM_WIRE Wire1 */

/* ── DVP (parallel) — used when CFG_CAM_BUS = DVP ── */
#ifndef CFG_DVP_SDA
#define CFG_DVP_SDA 7
#endif
#ifndef CFG_DVP_SCL
#define CFG_DVP_SCL 8
#endif
#ifndef CFG_DVP_PCLK
#define CFG_DVP_PCLK 4
#endif
#ifndef CFG_DVP_VSYNC
#define CFG_DVP_VSYNC 5
#endif
#ifndef CFG_DVP_DE
#define CFG_DVP_DE 6
#endif
#ifndef CFG_DVP_XCLK
#define CFG_DVP_XCLK 21
#endif
#ifndef CFG_DVP_WIDTH
#define CFG_DVP_WIDTH 8
#endif
#ifndef CFG_DVP_D0
#define CFG_DVP_D0 0
#define CFG_DVP_D1 1
#define CFG_DVP_D2 2
#define CFG_DVP_D3 3
#define CFG_DVP_D4 22
#define CFG_DVP_D5 23
#define CFG_DVP_D6 24
#define CFG_DVP_D7 25
#endif
#ifndef CFG_DVP_D8
#define CFG_DVP_D8 -1 /* 16-bit DVP only; leave -1 for 8-bit */
#define CFG_DVP_D9 -1
#define CFG_DVP_D10 -1
#define CFG_DVP_D11 -1
#define CFG_DVP_D12 -1
#define CFG_DVP_D13 -1
#define CFG_DVP_D14 -1
#define CFG_DVP_D15 -1
#endif

/* ── SPI camera — used when CFG_CAM_BUS = SPI ── */
#ifndef CFG_SPI_CS
#define CFG_SPI_CS -1
#define CFG_SPI_SCLK -1
#define CFG_SPI_D0 -1
#define CFG_SPI_D1 -1
#define CFG_SPI_D2 -1
#define CFG_SPI_D3 -1
#define CFG_SPI_XCLK -1
#define CFG_SPI_PORT 0
#define CFG_SPI_IO_MODE 0 /* 0=1-bit 1=2-bit 2=4-bit */
#define CFG_SPI_INTF 0    /* 0=SPI 1=PARLIO */
#endif

/* ── USB-host UVC — used when CFG_CAM_BUS = UVC_HOST ── */
#ifndef CFG_UVC_VID
#define CFG_UVC_VID 0
#define CFG_UVC_PID 0
#define CFG_UVC_DEV 0
#define CFG_UVC_FORMAT 1 /* 0=default 1=MJPEG 2=YUY2 */
#define CFG_UVC_W 640
#define CFG_UVC_H 480
#define CFG_UVC_FPS 0
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. microSD (SDMMC). 1-bit: CFG_SD_1BIT 1
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_SD_CLK
#define CFG_SD_CLK 43
#endif
#ifndef CFG_SD_CMD
#define CFG_SD_CMD 44
#endif
#ifndef CFG_SD_D0
#define CFG_SD_D0 39
#endif
#ifndef CFG_SD_D1
#define CFG_SD_D1 40
#endif
#ifndef CFG_SD_D2
#define CFG_SD_D2 41
#endif
#ifndef CFG_SD_D3
#define CFG_SD_D3 42
#endif
#ifndef CFG_SD_LDO_CHAN
#define CFG_SD_LDO_CHAN 4
#endif
#ifndef CFG_SD_1BIT
#define CFG_SD_1BIT 0
#endif
#ifndef CFG_SD_MOUNT
#define CFG_SD_MOUNT "/sdcard"
#endif
#ifndef CFG_SD_FREQ
#define CFG_SD_FREQ 0 /* 0 = SDMMC_FREQ_DEFAULT; or SDMMC_FREQ_HIGHSPEED */
#endif
#ifndef CFG_SD_FORMAT_IF_FAIL
#define CFG_SD_FORMAT_IF_FAIL 0
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. MIC — ES8311 I2C + I2S
 *    Waveshare P4-Nano: DIN=11  PA=53
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_MIC_TYPE
#define CFG_MIC_TYPE ESP32P4_MIC_ES8311
// #define CFG_MIC_TYPE ESP32P4_MIC_CUSTOM
#endif
#ifndef CFG_MIC_SDA
#define CFG_MIC_SDA 7
#endif
#ifndef CFG_MIC_SCL
#define CFG_MIC_SCL 8
#endif
#ifndef CFG_MIC_ADDR
#define CFG_MIC_ADDR 0x18
#endif
#ifndef CFG_MIC_MCLK
#define CFG_MIC_MCLK 13
#endif
#ifndef CFG_MIC_BCLK
#define CFG_MIC_BCLK 12
#endif
#ifndef CFG_MIC_WS
#define CFG_MIC_WS 10
#endif
#ifndef CFG_MIC_DOUT
#define CFG_MIC_DOUT 9
#endif
#ifndef CFG_MIC_DIN
#define CFG_MIC_DIN 48
#endif
#ifndef CFG_MIC_PA
#define CFG_MIC_PA 11
#endif
#ifndef CFG_MIC_RATE
#define CFG_MIC_RATE 16000
#endif
/* #define CFG_MIC_WIRE Wire1 */

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. WI-FI — ESP32-C6 SDIO (skip sketches if your board has no C6)
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_WIFI_SSID
#define CFG_WIFI_SSID "YourSSID"
#endif
#ifndef CFG_WIFI_PASS
#define CFG_WIFI_PASS "YourPassword"
#endif
#ifndef CFG_WIFI_HOSTNAME
#define CFG_WIFI_HOSTNAME "esp32p4-cam"
#endif
#ifndef CFG_WIFI_AP_SSID
#define CFG_WIFI_AP_SSID "ESP32-P4-Cam"
#endif
#ifndef CFG_WIFI_AP_PASS
#define CFG_WIFI_AP_PASS "camstream1"
#endif
#ifndef CFG_WIFI_TIMEOUT_MS
#define CFG_WIFI_TIMEOUT_MS 30000
#endif
#ifndef CFG_WIFI_C6_CLK
#define CFG_WIFI_C6_CLK 18
#endif
#ifndef CFG_WIFI_C6_CMD
#define CFG_WIFI_C6_CMD 19
#endif
#ifndef CFG_WIFI_C6_D0
#define CFG_WIFI_C6_D0 14
#endif
#ifndef CFG_WIFI_C6_D1
#define CFG_WIFI_C6_D1 15
#endif
#ifndef CFG_WIFI_C6_D2
#define CFG_WIFI_C6_D2 16
#endif
#ifndef CFG_WIFI_C6_D3
#define CFG_WIFI_C6_D3 17
#endif
#ifndef CFG_WIFI_C6_RST
#define CFG_WIFI_C6_RST 54
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. ETHERNET — RMII PHY (skip sketches if your board has no ETH jack)
 *    Pick ONE ETH_PHY_TYPE. These #defines must exist before ETH.h (included
 *    below). ESP32-P4 RMII data pins are core defaults, not GPIOs you remap:
 *      TXD0=34  TXD1=35  TX_EN=49  RXD0=30  RXD1=29  CRS_DV=28  REF_CLK=50
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef CFG_ETH_ADDR
#define CFG_ETH_ADDR 1
#endif
#ifndef CFG_ETH_MDC
#define CFG_ETH_MDC 31
#endif
#ifndef CFG_ETH_MDIO
#define CFG_ETH_MDIO 52
#endif
#ifndef CFG_ETH_POWER
#define CFG_ETH_POWER 51
#endif
#ifndef CFG_ETH_HOSTNAME
#define CFG_ETH_HOSTNAME "esp32p4-eth"
#endif
#ifndef CFG_ETH_TIMEOUT_MS
#define CFG_ETH_TIMEOUT_MS 30000
#endif

#ifndef ETH_PHY_MDC
#define ETH_PHY_TYPE ETH_PHY_IP101
// #define ETH_PHY_TYPE ETH_PHY_RTL8201
// #define ETH_PHY_TYPE ETH_PHY_LAN8720
// #define ETH_PHY_TYPE ETH_PHY_DP83848
// #define ETH_PHY_TYPE ETH_PHY_KSZ8041
// #define ETH_PHY_TYPE ETH_PHY_KSZ8081
// #define ETH_PHY_TYPE ETH_PHY_KSZ8851
// #define ETH_PHY_TYPE ETH_PHY_DM9051
// #define ETH_PHY_TYPE ETH_PHY_W5500
// #define ETH_PHY_TYPE ETH_PHY_JL1101
#define ETH_PHY_ADDR CFG_ETH_ADDR
#define ETH_PHY_MDC CFG_ETH_MDC
#define ETH_PHY_MDIO CFG_ETH_MDIO
#define ETH_PHY_POWER CFG_ETH_POWER
#define ETH_CLK_MODE EMAC_CLK_EXT_IN
// #define ETH_CLK_MODE EMAC_CLK_OUT
#endif

#include <ETH.h>

/* Aliases used by 15/16 es8311_m3.h */
#ifndef ESP32CSI_MIC_SDA
#define ESP32CSI_MIC_SDA CFG_MIC_SDA
#define ESP32CSI_MIC_SCL CFG_MIC_SCL
#define ESP32CSI_MIC_ADDR CFG_MIC_ADDR
#define ESP32CSI_MIC_MCLK CFG_MIC_MCLK
#define ESP32CSI_MIC_BCLK CFG_MIC_BCLK
#define ESP32CSI_MIC_WS CFG_MIC_WS
#define ESP32CSI_MIC_DOUT CFG_MIC_DOUT
#define ESP32CSI_MIC_DIN CFG_MIC_DIN
#define ESP32CSI_MIC_PA CFG_MIC_PA
#endif

/* ── helpers (used by the .ino next to this file) ───────────────────────── */

inline const char *esp32csi_board_name(esp32p4_board_t b = (esp32p4_board_t)ESP32CSI_BOARD) {
  switch (b) {
    case ESP32P4_BOARD_GUITION_M3: return "GUITION_M3";
    case ESP32P4_BOARD_WAVESHARE_NANO: return "WAVESHARE_NANO";
    case ESP32P4_BOARD_FUNCTION_EV: return "FUNCTION_EV";
    case ESP32P4_BOARD_CUSTOM: return "CUSTOM";
    default: return "?";
  }
}

inline const char *esp32csi_sensor_name(esp32p4_cam_sensor_t s) {
  switch (s) {
    case ESP32P4_SENSOR_AUTO: return "AUTO";
    case ESP32P4_SENSOR_SC2336: return "SC2336";
    case ESP32P4_SENSOR_OV5647: return "OV5647";
    case ESP32P4_SENSOR_OV5645: return "OV5645";
    case ESP32P4_SENSOR_OV2710: return "OV2710";
    case ESP32P4_SENSOR_OV9281: return "OV9281";
    case ESP32P4_SENSOR_SC202CS: return "SC202CS";
    case ESP32P4_SENSOR_SC1346: return "SC1346";
    case ESP32P4_SENSOR_SC030IOT: return "SC030IOT";
    case ESP32P4_SENSOR_SC035HGS: return "SC035HGS";
    case ESP32P4_SENSOR_OS02N10: return "OS02N10";
    case ESP32P4_SENSOR_OS04C10: return "OS04C10";
    case ESP32P4_SENSOR_GC2145: return "GC2145";
    case ESP32P4_SENSOR_STI2250: return "STI2250";
    case ESP32P4_SENSOR_SC121AT: return "SC121AT";
    case ESP32P4_SENSOR_MIRA220: return "MIRA220";
    case ESP32P4_SENSOR_IMX708: return "IMX708";
    case ESP32P4_SENSOR_IMX219: return "IMX219";
    case ESP32P4_SENSOR_IMX477: return "IMX477";
    case ESP32P4_SENSOR_IMX335: return "IMX335";
    case ESP32P4_SENSOR_IMX415: return "IMX415";
    case ESP32P4_SENSOR_GC2083: return "GC2083";
    case ESP32P4_SENSOR_GC2093: return "GC2093";
    case ESP32P4_SENSOR_OV7251: return "OV7251";
    case ESP32P4_SENSOR_IMX296: return "IMX296";
    case ESP32P4_SENSOR_IMX462: return "IMX462";
    case ESP32P4_SENSOR_ARDUCAM_IMX500: return "IMX500";
    case ESP32P4_SENSOR_SC2331: return "SC2331";
    case ESP32P4_SENSOR_GC2607: return "GC2607";
    case ESP32P4_SENSOR_OV5640: return "OV5640";
    case ESP32P4_SENSOR_LT6911: return "LT6911";
    case ESP32P4_SENSOR_OV2640: return "OV2640";
    case ESP32P4_SENSOR_SP0A39: return "SP0A39";
    default: return "?";
  }
}

inline const char *esp32csi_framesize_name(esp32p4_cam_framesize_t fs) {
  switch (fs) {
    case ESP32P4_FRAMESIZE_AUTO: return "AUTO";
    case ESP32P4_FRAMESIZE_VGA: return "VGA";
    case ESP32P4_FRAMESIZE_800X640: return "800x640";
    case ESP32P4_FRAMESIZE_SVGA: return "SVGA";
    case ESP32P4_FRAMESIZE_HD: return "HD";
    case ESP32P4_FRAMESIZE_SXGA: return "SXGA";
    case ESP32P4_FRAMESIZE_1080P: return "1080P";
    case ESP32P4_FRAMESIZE_2304X1296: return "2304x1296";
    case ESP32P4_FRAMESIZE_QXGA: return "QXGA";
    case ESP32P4_FRAMESIZE_5MP: return "5MP";
    default: return "?";
  }
}

inline esp32p4_cam_config_t esp32csi_cam_config() {
  esp32p4_cam_config_t c = esp32p4_cam_config_board((esp32p4_board_t)ESP32CSI_BOARD);
  c.bus = (esp32p4_cam_bus_t)CFG_CAM_BUS;
  c.sensor = (esp32p4_cam_sensor_t)CFG_CAM_SENSOR;
  c.frame_size = (esp32p4_cam_framesize_t)CFG_CAM_FRAMESIZE;
  c.pixel_format = (esp32p4_cam_pixformat_t)CFG_CAM_FORMAT;
  c.fb_count = CFG_CAM_FB_COUNT;
  c.lane_bit_rate_mbps = CFG_CAM_LANE_MBPS;
  c.csi_id = CFG_CAM_CSI_ID;
  c.sda = CFG_CAM_SDA;
  c.scl = CFG_CAM_SCL;
#ifdef CFG_CAM_WIRE
  c.wire = &CFG_CAM_WIRE;
#endif
  c.xclk = CFG_CAM_XCLK;
  c.xclk_hz = CFG_CAM_XCLK_HZ;
  c.pwdn = CFG_CAM_PWDN;
  c.reset = CFG_CAM_RESET;
  c.ldo_chan = CFG_CAM_LDO_CHAN;
  c.ldo_mv = CFG_CAM_LDO_MV;
  c.i2c_addr = CFG_CAM_I2C_ADDR;
  c.test_pattern = (CFG_CAM_TEST_PATTERN != 0);
  c.dvp.vsync = CFG_DVP_VSYNC;
  c.dvp.de = CFG_DVP_DE;
  c.dvp.pclk = CFG_DVP_PCLK;
  c.dvp.xclk = CFG_DVP_XCLK;
  c.dvp.data_width = CFG_DVP_WIDTH;
  c.dvp.data[0] = CFG_DVP_D0;
  c.dvp.data[1] = CFG_DVP_D1;
  c.dvp.data[2] = CFG_DVP_D2;
  c.dvp.data[3] = CFG_DVP_D3;
  c.dvp.data[4] = CFG_DVP_D4;
  c.dvp.data[5] = CFG_DVP_D5;
  c.dvp.data[6] = CFG_DVP_D6;
  c.dvp.data[7] = CFG_DVP_D7;
  c.dvp.data[8] = CFG_DVP_D8;
  c.dvp.data[9] = CFG_DVP_D9;
  c.dvp.data[10] = CFG_DVP_D10;
  c.dvp.data[11] = CFG_DVP_D11;
  c.dvp.data[12] = CFG_DVP_D12;
  c.dvp.data[13] = CFG_DVP_D13;
  c.dvp.data[14] = CFG_DVP_D14;
  c.dvp.data[15] = CFG_DVP_D15;
  c.spi.cs = CFG_SPI_CS;
  c.spi.sclk = CFG_SPI_SCLK;
  c.spi.d0 = CFG_SPI_D0;
  c.spi.d1 = CFG_SPI_D1;
  c.spi.d2 = CFG_SPI_D2;
  c.spi.d3 = CFG_SPI_D3;
  c.spi.xclk = CFG_SPI_XCLK;
  c.spi.spi_port = CFG_SPI_PORT;
  c.spi.io_mode = CFG_SPI_IO_MODE;
  c.spi.intf = CFG_SPI_INTF;
  c.uvc.vid = CFG_UVC_VID;
  c.uvc.pid = CFG_UVC_PID;
  c.uvc.dev_addr = CFG_UVC_DEV;
  c.uvc.format = CFG_UVC_FORMAT;
  c.uvc.width = CFG_UVC_W;
  c.uvc.height = CFG_UVC_H;
  c.uvc.fps = CFG_UVC_FPS;
  if (c.bus == ESP32P4_CAM_BUS_DVP) {
    c.sda = CFG_DVP_SDA;
    c.scl = CFG_DVP_SCL;
  }
  return c;
}

inline void esp32csi_print_cam_config(const esp32p4_cam_config_t &c) {
  const char *wire = (!c.wire || c.wire == &Wire) ? "Wire" : "Wire1";
  Serial.printf("CFG: cam board=%s bus=%u sensor=%s size=%s fmt=%s fb=%u\n", esp32csi_board_name(),
                (unsigned)c.bus, esp32csi_sensor_name(c.sensor), esp32csi_framesize_name(c.frame_size),
                esp32p4_pixformat_name(c.pixel_format), (unsigned)c.fb_count);
  Serial.printf("CFG: cam sda=%d scl=%d %s xclk=%d @%u pwdn=%d rst=%d ldo=%d/%dmV\n", c.sda, c.scl,
                wire, c.xclk, (unsigned)c.xclk_hz, c.pwdn, c.reset, c.ldo_chan, c.ldo_mv);
}

inline esp32p4_sd_config_t esp32csi_sd_config() {
  esp32p4_sd_config_t c = esp32p4_sd_config_board((esp32p4_board_t)ESP32CSI_BOARD);
  c.clk = CFG_SD_CLK;
  c.cmd = CFG_SD_CMD;
  c.d0 = CFG_SD_D0;
  c.d1 = CFG_SD_D1;
  c.d2 = CFG_SD_D2;
  c.d3 = CFG_SD_D3;
  c.ldo_chan = CFG_SD_LDO_CHAN;
  c.mode1bit = (CFG_SD_1BIT != 0);
  c.mountpoint = CFG_SD_MOUNT;
  c.format_if_mount_failed = (CFG_SD_FORMAT_IF_FAIL != 0);
  if (CFG_SD_FREQ > 0) c.frequency = CFG_SD_FREQ;
  return c;
}

inline void esp32csi_print_sd_config(const esp32p4_sd_config_t &c) {
  Serial.printf("CFG: sd clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d ldo=%d %s-bit %s\n", c.clk, c.cmd,
                c.d0, c.d1, c.d2, c.d3, c.ldo_chan, c.mode1bit ? "1" : "4",
                c.mountpoint ? c.mountpoint : "?");
}

inline esp32p4_mic_config_t esp32csi_mic_config() {
  esp32p4_mic_config_t c = esp32p4_mic_config_board((esp32p4_board_t)ESP32CSI_BOARD);
  c.type = (esp32p4_mic_type_t)CFG_MIC_TYPE;
  c.i2c_sda = CFG_MIC_SDA;
  c.i2c_scl = CFG_MIC_SCL;
  c.es8311_addr = (uint8_t)CFG_MIC_ADDR;
  c.i2s_mclk = CFG_MIC_MCLK;
  c.i2s_bclk = CFG_MIC_BCLK;
  c.i2s_ws = CFG_MIC_WS;
  c.i2s_dout = CFG_MIC_DOUT;
  c.i2s_din = CFG_MIC_DIN;
  c.pa_gpio = CFG_MIC_PA;
  c.sample_rate = CFG_MIC_RATE;
#ifdef CFG_MIC_WIRE
  c.wire = &CFG_MIC_WIRE;
#endif
  return c;
}

inline void esp32csi_print_mic_config(const esp32p4_mic_config_t &c) {
  const char *wire = (!c.wire || c.wire == &Wire) ? "Wire" : "Wire1";
  Serial.printf("CFG: mic %s sda=%d scl=%d addr=0x%02X mclk=%d bclk=%d ws=%d dout=%d din=%d pa=%d %dHz\n",
                wire, c.i2c_sda, c.i2c_scl, (unsigned)c.es8311_addr, c.i2s_mclk, c.i2s_bclk, c.i2s_ws,
                c.i2s_dout, c.i2s_din, c.pa_gpio, c.sample_rate);
}

struct esp32csi_wifi_config_t {
  const char *ssid;
  const char *pass;
  const char *hostname;
  const char *ap_ssid;
  const char *ap_pass;
  int clk, cmd, d0, d1, d2, d3, rst;
};

inline esp32csi_wifi_config_t esp32csi_wifi_config() {
  esp32csi_wifi_config_t c{};
  c.ssid = CFG_WIFI_SSID;
  c.pass = CFG_WIFI_PASS;
  c.hostname = CFG_WIFI_HOSTNAME;
  c.ap_ssid = CFG_WIFI_AP_SSID;
  c.ap_pass = CFG_WIFI_AP_PASS;
  c.clk = CFG_WIFI_C6_CLK;
  c.cmd = CFG_WIFI_C6_CMD;
  c.d0 = CFG_WIFI_C6_D0;
  c.d1 = CFG_WIFI_C6_D1;
  c.d2 = CFG_WIFI_C6_D2;
  c.d3 = CFG_WIFI_C6_D3;
  c.rst = CFG_WIFI_C6_RST;
  return c;
}

inline void esp32csi_print_wifi_config(const esp32csi_wifi_config_t &c) {
  Serial.printf("CFG: wifi ssid=%s host=%s\n", c.ssid ? c.ssid : "", c.hostname ? c.hostname : "");
  Serial.printf("CFG: wifi c6 clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d rst=%d\n", c.clk, c.cmd, c.d0,
                c.d1, c.d2, c.d3, c.rst);
}

inline bool esp32csi_wifi_begin(const esp32csi_wifi_config_t &c = esp32csi_wifi_config()) {
  WiFi.setPins(c.clk, c.cmd, c.d0, c.d1, c.d2, c.d3, c.rst);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (c.hostname && c.hostname[0]) WiFi.setHostname(c.hostname);
  Serial.printf("CFG: Wi-Fi STA \"%s\" ...\n", c.ssid ? c.ssid : "");
  WiFi.begin(c.ssid, c.pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < (uint32_t)CFG_WIFI_TIMEOUT_MS) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("CFG: Wi-Fi ");
    Serial.println(WiFi.localIP());
    return true;
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(c.ap_ssid, c.ap_pass);
  Serial.printf("CFG: SoftAP \"%s\" ", c.ap_ssid ? c.ap_ssid : "");
  Serial.println(WiFi.softAPIP());
  return true;
}

inline IPAddress esp32csi_wifi_ip() {
  return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
}

struct esp32csi_eth_config_t {
  int phy_addr;
  int mdc;
  int mdio;
  int power;
  const char *hostname;
  uint32_t timeout_ms;
};

inline esp32csi_eth_config_t esp32csi_eth_config() {
  esp32csi_eth_config_t c{};
  c.phy_addr = CFG_ETH_ADDR;
  c.mdc = CFG_ETH_MDC;
  c.mdio = CFG_ETH_MDIO;
  c.power = CFG_ETH_POWER;
  c.hostname = CFG_ETH_HOSTNAME;
  c.timeout_ms = CFG_ETH_TIMEOUT_MS;
  return c;
}

inline void esp32csi_print_eth_config(const esp32csi_eth_config_t &c) {
  Serial.printf("CFG: eth mdc=%d mdio=%d power=%d phy_addr=%d host=%s\n", c.mdc, c.mdio, c.power,
                c.phy_addr, c.hostname ? c.hostname : "");
}

inline WfmEthConfig esp32csi_wfm_eth() {
  WfmEthConfig c;
  c.phyType = ETH_PHY_TYPE;
  c.phyAddr = CFG_ETH_ADDR;
  c.mdc = CFG_ETH_MDC;
  c.mdio = CFG_ETH_MDIO;
  c.power = CFG_ETH_POWER;
  c.clkMode = ETH_CLK_MODE;
  return c;
}

inline bool esp32csi_eth_begin(const esp32csi_eth_config_t &c = esp32csi_eth_config()) {
  Serial.printf("CFG: ETH phy=%d addr=%d mdc=%d mdio=%d power=%d\n", (int)ETH_PHY_TYPE, c.phy_addr,
                c.mdc, c.mdio, c.power);
  if (!ETH.begin(ETH_PHY_TYPE, c.phy_addr, c.mdc, c.mdio, c.power, ETH_CLK_MODE)) {
    Serial.println("CFG: ETH.begin FAILED — PHY type / RMII pins in board_config.h");
    return false;
  }
  return true;
}

inline void esp32csi_print_board() {
  Serial.println("CFG: --- edit board_config.h in THIS example folder ---");
  Serial.printf("CFG: ESP32CSI_BOARD=%s\n", esp32csi_board_name());
  esp32csi_print_cam_config(esp32csi_cam_config());
  esp32csi_print_sd_config(esp32csi_sd_config());
  esp32csi_print_mic_config(esp32csi_mic_config());
  esp32csi_print_wifi_config(esp32csi_wifi_config());
  esp32csi_print_eth_config(esp32csi_eth_config());
}
