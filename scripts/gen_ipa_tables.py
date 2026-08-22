#!/usr/bin/env python3
"""Bake Espressif IPA JSON (ESPRESSIF MIT) into C tables for ESP32CSI_Vision.

Only RAW sensors we actually run through the P4 ISP. YUV/RGB (OV5645, GC2145,
SC121AT) skip IPA. No JSON: IMX708/IMX219 (Pi IPA is not license-clean).
SC030IOT has no JSON — runtime aliases SC035HGS.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

IDENT9 = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
BF9 = [1, 2, 1, 2, 4, 2, 1, 2, 1]
SH9 = [1, 2, 1, 2, 2, 2, 1, 2, 1]

# subdir, json filename, JSON object key, C prefix, bake LSC
SENSORS = [
    ("sc2336", "sc2336_default_p4_eco5.json", "SC2336", "Sc2336", True),
    ("ov5647", "ov5647_default.json", "OV5647", "Ov5647", False),
    ("ov2710", "ov2710_default.json", "OV2710", "Ov2710", False),
    ("ov9281", "ov9281_default_p4_eco5.json", "OV9281", "Ov9281", False),
    ("sc202cs", "sc202cs_default.json", "SC202CS", "Sc202cs", True),
    ("sc1346", "sc1346_default.json", "SC1346", "Sc1346", True),
    ("sc035hgs", "sc035hgs_rgb_default.json", "SC035HGS", "Sc035hgs", False),
    ("sc2331", "sc2331_default.json", "SC2331", "Sc2331", False),
    ("gc2607", "gc2607_default.json", "GC2607", "Gc2607", False),
    ("os02n10", "os02n10_default_p4_eco5.json", "OS02N10", "Os02n10", True),
    ("os04c10", "os04c10_default.json", "OS04C10", "Os04c10", True),
    ("sti2250", "sti2250_default.json", "STI2250", "Sti2250", True),
    ("mira220", "mira220_default.json", "MIRA220", "Mira220", False),
]


def pack_lsc(f: float) -> int:
    f = max(0.0, min(3.99609375, float(f)))
    i = int(f)
    d = int(round((f - i) * 256.0))
    if d >= 256:
        i += 1
        d = 0
    if i > 3:
        i, d = 3, 255
    return (i << 8) | d


def find_sensors() -> Path:
    env = os.environ.get("ESP_CAM_SENSOR_CFG")
    if env and Path(env).is_dir():
        return Path(env)
    env2 = os.environ.get("ESP_VIDEO_COMPONENTS")
    if env2:
        p = Path(env2) / "esp_cam_sensor" / "sensors"
        if p.is_dir():
            return p
    temp = Path(os.environ.get("TEMP", "/tmp")) / "esp-video-components" / "esp_cam_sensor" / "sensors"
    if temp.is_dir():
        return temp
    raise SystemExit("esp_cam_sensor JSON not found. Set ESP_VIDEO_COMPONENTS.")


def emit_u16_array(name: str, vals: list[int], cols: int = 16) -> str:
    lines = [f"static const uint16_t {name}[] = {{"]
    for i in range(0, len(vals), cols):
        chunk = ", ".join(f"0x{v:04x}" for v in vals[i : i + cols])
        lines.append(f"  {chunk},")
    lines.append("};")
    return "\n".join(lines) + "\n"


def emit_float9(m: list[float]) -> str:
    return ", ".join(f"{float(x):.6f}f" for x in m)


def first(seq, default=None):
    return seq[0] if seq else default


def write_sensor(src: Path, key: str, pfx: str, out: Path, bake_lsc: bool) -> None:
    root = json.loads(src.read_text(encoding="utf-8"))
    s = root[key]
    acc = s.get("acc") or {}
    awb = s.get("awb") or {}
    agc = s.get("agc") or {}
    aen = s.get("aen") or {}
    adn = s.get("adn") or {}
    ccm = acc.get("ccm") or {}
    ccm_tbl = ccm.get("table") or []
    ll = ccm.get("low_luma") or {}
    lsc = acc.get("lsc") if bake_lsc else None

    parts = [
        f"/* Generated from Espressif {src.name} (ESPRESSIF MIT). */",
        "#pragma once",
        "#include <stdint.h>",
        "",
    ]

    if isinstance(lsc, dict) and lsc.get("table"):
        sets = []
        for i, t in enumerate(lsc["table"]):
            for ch, ck in enumerate(
                ("calibrations_r_tbl", "calibrations_gr_tbl", "calibrations_gb_tbl", "calibrations_b_tbl")
            ):
                packed = [pack_lsc(x) for x in t[ck]]
                parts.append(emit_u16_array(f"k{pfx}Lsc{i}c{ch}", packed))
            sets.append(int(t["ct"]))
        nsets = len(sets)
        parts.append(f"static const uint16_t k{pfx}LscW = {int(lsc['img_w'])};")
        parts.append(f"static const uint16_t k{pfx}LscH = {int(lsc['img_h'])};")
        parts.append(f"static const uint16_t k{pfx}LscN = {int(lsc['lsc_tbl_size'])};")
        parts.append(f"static const uint8_t k{pfx}LscSets = {nsets};")
        parts.append(f"static const uint16_t k{pfx}LscCt[] = {{ " + ", ".join(str(c) for c in sets) + " };")
        parts.append(f"static const uint16_t *const k{pfx}LscCh[{nsets}][4] = {{")
        for i in range(nsets):
            parts.append(f"  {{ k{pfx}Lsc{i}c0, k{pfx}Lsc{i}c1, k{pfx}Lsc{i}c2, k{pfx}Lsc{i}c3 }},")
        parts.append("};")
        parts.append(f"static const uint8_t k{pfx}HasLsc = 1;")
    else:
        parts.append(f"static const uint8_t k{pfx}HasLsc = 0;")
        parts.append(f"static const uint16_t k{pfx}LscW = 0;")
        parts.append(f"static const uint16_t k{pfx}LscH = 0;")
        parts.append(f"static const uint16_t k{pfx}LscN = 0;")
        parts.append(f"static const uint8_t k{pfx}LscSets = 0;")
        parts.append(f"static const uint16_t k{pfx}LscCt[] = {{ 0 }};")
        parts.append(f"static const uint16_t *const k{pfx}LscCh[1][4] = {{{{ 0, 0, 0, 0 }}}};")

    parts.append("")
    parts.append(f"static const float k{pfx}Ccm[][10] = {{")
    if ccm_tbl:
        for t in ccm_tbl:
            m = t["matrix"]
            parts.append(f"  {{ {float(t.get('color_temp', 0)):.1f}f, {emit_float9(m)} }},")
    else:
        parts.append(f"  {{ 5000.0f, {emit_float9(IDENT9)} }},")
    parts.append("};")
    parts.append(f"static const uint8_t k{pfx}CcmN = {max(1, len(ccm_tbl))};")
    llm = ll.get("matrix") or IDENT9
    parts.append(f"static const float k{pfx}LowLumaThr = {float(ll.get('threshold', 28)):.1f}f;")
    parts.append(f"static const float k{pfx}LowLumaM[9] = {{ {emit_float9(llm)} }};")
    parts.append("")

    refs = awb.get("ref_points") or []
    parts.append(f"static const float k{pfx}AwbRef[][4] = {{")
    if refs:
        for r in refs:
            parts.append(
                f"  {{ {float(r['ct']):.1f}f, {float(r['rg']):.6f}f, {float(r['bg']):.6f}f, {float(r.get('radius', 0)):.4f}f }},"
            )
    else:
        parts.append("  { 0.0f, 0.0f, 0.0f, 0.0f },")
    parts.append("};")
    parts.append(f"static const uint8_t k{pfx}AwbRefN = {len(refs)};")
    zone_map = {"uhct": 0, "hct": 1, "mct": 2, "lct": 3, "ulct": 4, "green": 5, "skin": 6}
    zones = awb.get("zones") or []
    parts.append(f"static const float k{pfx}AwbZone[][7] = {{")
    if zones:
        for z in zones:
            rg = z.get("rg") or {}
            bg = z.get("bg") or {}
            zt = zone_map.get(str(z.get("type", "")).lower(), 2)
            en = 1 if z.get("enabled", True) else 0
            parts.append(
                f"  {{ {float(zt):.1f}f, {float(rg.get('min', 0)):.4f}f, {float(rg.get('max', 1)):.4f}f, "
                f"{float(bg.get('min', 0)):.4f}f, {float(bg.get('max', 1)):.4f}f, {float(en):.1f}f, 0.0f }},"
            )
    else:
        parts.append("  { 0 },")
    parts.append("};")
    parts.append(f"static const uint8_t k{pfx}AwbZoneN = {len(zones)};")
    rng = awb.get("range") or {}
    rg = rng.get("rg") or {}
    bg = rng.get("bg") or {}
    gn = rng.get("green") or {}
    parts.append(f"static const float k{pfx}RgMin = {float(rg.get('min', 0.32)):.4f}f;")
    parts.append(f"static const float k{pfx}RgMax = {float(rg.get('max', 0.97)):.4f}f;")
    parts.append(f"static const float k{pfx}BgMin = {float(bg.get('min', 0.22)):.4f}f;")
    parts.append(f"static const float k{pfx}BgMax = {float(bg.get('max', 0.80)):.4f}f;")
    parts.append(f"static const uint8_t k{pfx}GMin = {int(gn.get('min', 16))};")
    parts.append(f"static const uint8_t k{pfx}GMax = {int(gn.get('max', 220))};")
    parts.append(f"static const float k{pfx}NewW = {float(awb.get('new_w', 0.30)):.3f}f;")
    parts.append(f"static const float k{pfx}PrevW = {float(awb.get('prev_w', 0.70)):.3f}f;")
    parts.append(f"static const float k{pfx}RScale = {float(awb.get('red_gain_scale', 1.0)):.3f}f;")
    parts.append(f"static const float k{pfx}BScale = {float(awb.get('blue_gain_scale', 1.0)):.3f}f;")
    parts.append(f"static const uint32_t k{pfx}MinCounted = {int(awb.get('min_counted', 80))};")

    la = agc.get("luma_adjust") or {}
    ac = agc.get("anti_flicker") or {}
    parts.append(f"static const uint8_t k{pfx}AeTarget = {int(la.get('target', 80))};")
    parts.append(f"static const uint8_t k{pfx}AeLow = {int(la.get('target_low', 70))};")
    parts.append(f"static const uint8_t k{pfx}AeHigh = {int(la.get('target_high', 90))};")
    parts.append(f"static const uint8_t k{pfx}AeHiThr = {int(la.get('high_threshold', 239))};")
    parts.append(f"static const uint8_t k{pfx}AeHiReg = {int(la.get('high_regions', 3))};")
    parts.append(f"static const uint8_t k{pfx}AeLoThr = {int(la.get('low_threshold', 13))};")
    parts.append(f"static const uint8_t k{pfx}AeLoReg = {int(la.get('low_regions', 5))};")
    wt = la.get("weight") or [1] * 25
    wt = [int(x) for x in wt[:25]]
    while len(wt) < 25:
        wt.append(1)
    parts.append(f"static const uint8_t k{pfx}AeWt[25] = {{ " + ", ".join(str(x) for x in wt) + " };")
    hz = int(ac.get("ac_freq", 50) or 50)
    if str(ac.get("mode", "none")).lower() == "none":
        hz = 0
    parts.append(f"static const uint8_t k{pfx}AcHz = {hz};")
    parts.append(f"static const float k{pfx}IncR = {float(agc.get('f_n0', 0.32)):.4f}f;")
    parts.append(f"static const float k{pfx}DecR = {float(agc.get('f_m0', agc.get('f_n0', 0.42))):.4f}f;")
    ian = ((s.get("ian") or {}).get("luma") or {}).get("env") or {}
    parts.append(f"static const float k{pfx}EnvK = {float(ian.get('k', 0) or 0):.1f}f;")
    sp = ian.get("speed_param") or []
    if sp:
        parts.append(
            f"static const float k{pfx}EnvSp[] = {{ " + ", ".join(f"{float(x):.6f}f" for x in sp[:16]) + " };"
        )
        parts.append(f"static const uint8_t k{pfx}EnvSpN = {min(16, len(sp))};")
    else:
        parts.append(f"static const float k{pfx}EnvSp[] = {{ 0 }};")
        parts.append(f"static const uint8_t k{pfx}EnvSpN = 0;")
    pwl = agc.get("luma_pwl") or {}
    ptab = pwl.get("table") or [] if pwl.get("enable", False) else []
    parts.append(f"static const float k{pfx}Pwl[][2] = {{")
    if ptab:
        for e in ptab:
            parts.append(f"  {{ {float(e.get('env_luma', 0)):.2f}f, {float(e.get('luma_shift', 0)):.1f}f }},")
    else:
        parts.append("  { 0.0f, 0.0f },")
    parts.append("};")
    parts.append(f"static const uint8_t k{pfx}PwlN = {len(ptab)};")
    blc = acc.get("blc") or {}
    btab = blc.get("blc_table") or []
    parts.append(f"static const float k{pfx}Blc[][5] = {{")
    if btab:
        for e in btab:
            bp = e.get("blc_param") or {}
            parts.append(
                f"  {{ {float(e.get('gain', 1)):.3f}f, {float(bp.get('blc_top_left', 16)):.1f}f, "
                f"{float(bp.get('blc_top_right', 16)):.1f}f, {float(bp.get('blc_bottom_left', 16)):.1f}f, "
                f"{float(bp.get('blc_bottom_right', 16)):.1f}f }},"
            )
    else:
        parts.append("  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },")
    parts.append("};")
    parts.append(f"static const uint8_t k{pfx}BlcN = {max(1, len(btab))};")
    parts.append(f"static const uint8_t k{pfx}BlcStretch = {1 if blc.get('stretch') else 0};")

    gtab = (aen.get("gamma") or {}).get("table") or []
    if gtab:
        mid = gtab[len(gtab) // 2]
        g = float(mid.get("gamma_param", 0.55) or 0.55)
    else:
        g = 0.55
    parts.append(f"static const float k{pfx}Gamma = {g:.4f}f;")

    sh = ((first(aen.get("sharpen") or []) or {}).get("param")) or {}
    sm = sh.get("matrix") or SH9
    parts.append(f"static const uint8_t k{pfx}ShH = {int(sh.get('h_thresh', 20))};")
    parts.append(f"static const uint8_t k{pfx}ShL = {int(sh.get('l_thresh', 4))};")
    parts.append(f"static const float k{pfx}ShHc = {float(sh.get('h_coeff', 1.5)):.4f}f;")
    parts.append(f"static const float k{pfx}ShMc = {float(sh.get('m_coeff', 1.25)):.4f}f;")
    parts.append(f"static const uint8_t k{pfx}ShMat[9] = {{ " + ", ".join(str(int(x)) for x in sm) + " };")
    bf = ((first(adn.get("bf") or []) or {}).get("param")) or {}
    bm = bf.get("matrix") or BF9
    parts.append(f"static const uint8_t k{pfx}BfLevel = {int(bf.get('level', 4))};")
    parts.append(f"static const uint8_t k{pfx}BfMat[9] = {{ " + ", ".join(str(int(x)) for x in bm) + " };")
    sat = acc.get("saturation") or []
    sat_v = int(sat[-1]["value"]) if sat else 128
    con = int((first(aen.get("contrast") or []) or {}).get("value", 128))
    parts.append(f"static const uint8_t k{pfx}Sat = {sat_v};")
    parts.append(f"static const uint8_t k{pfx}Contrast = {con};")

    out.write_text("\n".join(parts) + "\n", encoding="utf-8")
    print("wrote", out.name, "bytes", out.stat().st_size, "lsc" if (isinstance(lsc, dict) and lsc.get("table")) else "no-lsc")


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    dest = root / "src" / "cam" / "ipa"
    dest.mkdir(parents=True, exist_ok=True)
    sensors = find_sensors()
    for sub, fn, key, pfx, lsc in SENSORS:
        src = sensors / sub / "cfg" / fn
        if not src.is_file():
            raise SystemExit(f"missing {src}")
        write_sensor(src, key, pfx, dest / f"esp32p4_ipa_{sub}.h", lsc)


if __name__ == "__main__":
    main()
