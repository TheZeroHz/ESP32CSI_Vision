#!/usr/bin/env python3
import os, re

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sens = os.path.join(root, "src", "cam", "sensors")
tmp = os.path.join(root, ".tmp_esp_cam_sensor", "repo", "esp_cam_sensor", "sensors")

specs = [
  ("OV9281","ov9281",0x9281,0x300a,[0x60,0x10],"ov9281_mipi_2lane_24Minput_1280x720_raw8_50fps.h",1280,720,2,400,"BGGR","RAW8",0x0100,0x01,0x00,"FULL"),
  ("OS02N10","os02n10",0x534e,0x300a,[0x3c,0x3d],"os02n10_mipi_2lane_24Minput_1920x1080_raw10_25fps.h",1920,1080,2,480,"BGGR","RAW10",0x0100,0x01,0x00,"FULL"),
  ("SC035HGS","sc035hgs",0x0031,0x3107,[0x30],"sc035hgs_mipi_2lane_24Minput_640x480_raw8_linear_50fps.h",640,480,2,360,"BGGR","RAW8",0x0100,0x01,0x00,"FULL"),
  ("OV2710","ov2710",0x2710,0x300a,[0x36],"ov2710_mipi_1lane_24Minput_1920x1080_raw10_25fps.h",1920,1080,1,800,"BGGR","RAW10",0x0100,0x01,0x00,"EXPERIMENTAL"),
  ("SC202CS","sc202cs",0xeb52,0x3107,[0x36],"sc202cs_mipi_1lane_24Minput_1280x720_raw8_30fps.h",1280,720,1,360,"BGGR","RAW8",0x0100,0x01,0x00,"FULL"),
  ("SC1346","sc1346",0x003a,0x3107,[0x30],"sc1346_mipi_1lane_24Minput_720p_raw10_30fps.h",1280,720,1,480,"BGGR","RAW10",0x0100,0x01,0x00,"FULL"),
  ("SC030IOT","sc030iot",0x9a46,0x3107,[0x68],"sc030iot_mipi_1lane_24Minput_640x480_raw8_60fps.h",640,480,1,240,"BGGR","RAW8",0x0100,0x01,0x00,"EXPERIMENTAL"),
  ("OS04C10","os04c10",0x5304,0x300a,[0x36,0x10],"os04c10_mipi_1lane_24Minput_960x1280_raw10_30fps.h",960,1280,1,800,"BGGR","RAW10",0x0100,0x01,0x00,"EXPERIMENTAL"),
  ("STI2250","sti2250",0x2250,0x0000,[0x37,0x10],"sti2250_mipi_1lane_24Minput_raw8_800x600_50fps.h",800,600,1,400,"BGGR","RAW8",0x0100,0x01,0x00,"EXPERIMENTAL"),
  ("MIRA220","mira220",0x0131,0x0000,[0x54],"mira220_mipi_2lane_24Minput_1024x600_raw8_15fps.h",1024,600,2,400,"BGGR","RAW8",0x0100,0x01,0x00,"EXPERIMENTAL"),
]

pid_fixes = {}
idreg_fixes = {}
for folder in os.listdir(tmp):
    h = os.path.join(tmp, folder, "include", f"{folder}.h")
    if os.path.isfile(h):
        txt = open(h, encoding="utf-8", errors="ignore").read()
        m = re.search(r"#define\s+\w+_PID\s+0x([0-9a-fA-F]+)", txt)
        if m:
            pid_fixes[folder] = int(m.group(1), 16)
    rh = os.path.join(tmp, folder, "private_include", f"{folder}_regs.h")
    if os.path.isfile(rh):
        txt = open(rh, encoding="utf-8", errors="ignore").read()
        m = re.search(r"#define\s+\w+_REG_(?:SENSOR_ID_H|CHIP_ID_H)\s+0x([0-9a-fA-F]+)", txt)
        if m:
            idreg_fixes[folder] = int(m.group(1), 16)

out = ['''/*
 * Auto-generated Arduino MIPI RAW sensor wrappers.
 * Register tables: SPDX-License-Identifier: Apache-2.0 (Espressif Systems).
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include <Arduino.h>
''']
aliases = []
for name, folder, pid, id_reg, addrs, mode, w, h, lanes, mbps, bayer, infmt, sreg, son, soff, support in specs:
    if folder in pid_fixes:
        pid = pid_fixes[folder]
    if folder in idreg_fixes:
        id_reg = idreg_fixes[folder]
    mode_path = os.path.join(sens, folder, mode)
    if not os.path.isfile(mode_path):
        print("MISSING", mode_path)
        continue
    mt = open(mode_path, encoding="utf-8", errors="ignore").read()
    tm = re.search(r"static const (\w+)\s+(\w+)\[", mt)
    if not tm:
        print("no table", name)
        continue
    arr = tm.group(2)
    types_h = f"{folder}_types.h"
    regs_h = f"{folder}_regs.h"
    enum_id = f"ESP32P4_SENSOR_{name}"
    support_e = f"ESP32P4_CAM_SUPPORT_{support}"
    bayer_e = f"ESP32P4_BAYER_{bayer}"
    infmt_e = f"ESP32P4_CAM_IN_{infmt}"
    addr_list = ", ".join(f"0x{a:02X}" for a in addrs) + ", 0"
    fs_tag = (
        "ESP32P4_FRAMESIZE_1080P" if h >= 1080 else
        "ESP32P4_FRAMESIZE_HD" if h >= 720 else
        "ESP32P4_FRAMESIZE_SVGA" if h >= 600 else
        "ESP32P4_FRAMESIZE_VGA"
    )
    ns = name.lower()
    stream_on = f"return esp32p4_sccb_write8(addr7, 0x{sreg:04X}, 0x{son:02X});"
    stream_off = f"return esp32p4_sccb_write8(addr7, 0x{sreg:04X}, 0x{soff:02X});"
    inc_regs = ""
    if os.path.isfile(os.path.join(sens, folder, regs_h)):
        # Only include regs if needed by table macros
        if f"{folder.upper()}_REG_" in mt or "REG_SLEEP" in mt or "REG_END" in mt:
            inc_regs = f'#include "cam/sensors/{folder}/{regs_h}"\n'
    # Many tables reference REG_END macros from regs.h
    if "REG_END" in mt:
        inc_regs = f'#include "cam/sensors/{folder}/{regs_h}"\n'
    block = f'''
/* ---- {name} pid=0x{pid:04X} ---- */
namespace {ns}_drv {{
#include "cam/sensors/{folder}/{types_h}"
{inc_regs}#include "cam/sensors/{folder}/{mode}"
static const uint8_t kAddrs[] = {{{addr_list}}};
static bool detect(uint8_t *addr7_out) {{
  for (const uint8_t *p = kAddrs; *p; ++p) {{
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x{id_reg:04X}, &got)) continue;
    if (got == 0x{pid:04X}) {{ if (addr7_out) *addr7_out = *p; return true; }}
  }}
  return false;
}}
static bool stream_on(uint8_t addr7) {{ {stream_on} }}
static bool stream_off(uint8_t addr7) {{ {stream_off} }}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {{
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *){arr};
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {{
    mode_out->name = "{name} {w}x{h}";
    mode_out->width = {w}; mode_out->height = {h}; mode_out->lanes = {lanes};
    mode_out->lane_mbps = {mbps}; mode_out->in_fmt = {infmt_e}; mode_out->bayer = {bayer_e};
    mode_out->framesize_tag = {fs_tag}; mode_out->regs = regs; mode_out->regs_count = n;
  }}
  return true;
}}
static const esp32p4_cam_sensor_ops_t kOps = {{
  {enum_id}, "{name}", {support_e}, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
}};
}}  // namespace
const esp32p4_cam_sensor_ops_t *{ns}_sensor_ops(void) {{ return &{ns}_drv::kOps; }}
'''
    out.append(block)
    aliases.append(ns)
    print("OK", name, f"pid=0x{pid:04X}", f"id@0x{id_reg:04X}")

out_cpp = os.path.join(sens, "esp32p4_mipi_raw_sensors.cpp")
open(out_cpp, "w", encoding="utf-8").write("\n".join(out))
print("Wrote", out_cpp)

hdr = os.path.join(sens, "esp32p4_sensor_ops_decl.h")
with open(hdr, "w", encoding="utf-8") as f:
    f.write("#pragma once\n#include \"cam/esp32p4_cam_sensor_ops.h\"\n")
    f.write("const esp32p4_cam_sensor_ops_t *ov5647_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *imx708_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *sc2336_sensor_ops(void);\n")
    for ns in aliases:
        f.write(f"const esp32p4_cam_sensor_ops_t *{ns}_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *ov5645_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *gc2145_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *sc121at_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *imx219_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *imx477_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *gc2083_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *gc2093_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *imx335_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *imx415_sensor_ops(void);\n")
    f.write("const esp32p4_cam_sensor_ops_t *detect_stubs_ops_list(size_t *count);\n")
print("decls", hdr)
