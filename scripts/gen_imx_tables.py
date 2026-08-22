#!/usr/bin/env python3
"""Extract IMX477 mode tables from a raspberrypi/linux imx477.c dump."""
from __future__ import annotations

import re
import sys
from pathlib import Path

PAIR = re.compile(r"\{0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+)\}")


def extract_array(text: str, name: str) -> list[tuple[int, int]]:
    m = re.search(rf"static const struct imx477_reg {name}\[\] = \{{(.*?)\n\}};", text, re.S)
    if not m:
        raise SystemExit(f"missing array {name}")
    out = [(int(a, 16), int(b, 16)) for a, b in PAIR.findall(m.group(1))]
    if not out:
        raise SystemExit(f"empty array {name}")
    return out


def emit(rows: list[tuple[int, int]], indent: str = "    ") -> str:
    lines = []
    for i in range(0, len(rows), 4):
        chunk = rows[i : i + 4]
        parts = [f"{{0x{r:04X}, 0x{v:02X}}}" for r, v in chunk]
        lines.append(indent + ", ".join(parts) + ",")
    return "\n".join(lines)


def patch(rows: list[tuple[int, int]], updates: dict[int, int]) -> list[tuple[int, int]]:
    out = []
    seen = set()
    for r, v in rows:
        if r in updates:
            v = updates[r]
            seen.add(r)
        out.append((r, v))
    for r, v in updates.items():
        if r not in seen:
            out.append((r, v))
    return out


def main() -> None:
    src = Path(sys.argv[1])
    dest = Path(sys.argv[2])
    text = src.read_text(encoding="utf-8", errors="replace")
    common = extract_array(text, "mode_common_regs")
    mode_1080 = extract_array(text, "mode_2028x1080_regs")
    mode_990 = extract_array(text, "mode_1332x990_regs")

    # P4 ISP max is 1920x1080 and the CSI path is RAW10.
    common = patch(common, {0x0112: 0x0A, 0x0113: 0x0A, 0x0114: 0x01})
    mode_1080 = patch(
        mode_1080,
        {
            0x0112: 0x0A,
            0x0113: 0x0A,
            0x0309: 0x0A,
            0x034C: 0x07,
            0x034D: 0x80,  # 1920
            0x0408: 0x00,
            0x0409: 0x36,  # center-crop 2028→1920
            0x040C: 0x07,
            0x040D: 0x80,
            0x034E: 0x04,
            0x034F: 0x38,  # 1080
        },
    )

    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(
        f"""#pragma once
#include "cam/esp32p4_cam_sensor_ops.h"

/*
 * IMX477 / IMX378 2-lane RAW10 tables for ESP32-P4.
 * Register map follows the public Raspberry Pi HQ Camera driver
 * (drivers/media/i2c/imx477.c). Values are rewritten here for P4:
 * 10-bit CSI (not 12-bit) and 1920x1080 crop (ISP max).
 */
#define IMX477_REG_CHIP_ID      0x0016
#define IMX477_CHIP_ID          0x0477
#define IMX378_CHIP_ID          0x0378
#define IMX477_REG_MODE_SELECT  0x0100
#define IMX477_MODE_STANDBY     0x00
#define IMX477_MODE_STREAMING   0x01
#define IMX477_REG_ORIENTATION  0x0101
#define IMX477_REG_EXPOSURE     0x0202
#define IMX477_REG_ANA_GAIN     0x0204
#define IMX477_REG_FRAME_LENGTH 0x0340

static const esp32p4_reg8_t imx477_common[] = {{
{emit(common)}
    {{0xFFFF, 0x00}},
}};

static const esp32p4_reg8_t imx477_link_450mhz[] = {{
    {{0x030E, 0x00}}, {{0x030F, 0x96}},
    {{0xFFFF, 0x00}},
}};

static const esp32p4_reg8_t imx477_1920x1080_raw10[] = {{
{emit(mode_1080)}
    {{0xFFFF, 0x00}},
}};

static const esp32p4_reg8_t imx477_1332x990_raw10[] = {{
{emit(mode_990)}
    {{0xFFFF, 0x00}},
}};
""",
        encoding="utf-8",
    )
    print(f"wrote {dest} common={len(common)} 1080={len(mode_1080)} 990={len(mode_990)}")


if __name__ == "__main__":
    main()
