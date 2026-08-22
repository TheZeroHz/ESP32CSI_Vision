#include "stream/ESP32P4_Mjpeg.h"

#include "debug/ESP32P4_Debug.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"

#include <ctype.h>
#include <errno.h>
#include <esp_wifi.h>
#include <stdio.h>
#include <string.h>
#include <lwip/sockets.h>

static bool mjpeg_is_rec_work(const char *name) {
  if (!name || !name[0]) return false;
  const char *base = strrchr(name, '/');
  base = base ? base + 1 : name;
  size_t n = strlen(base);
  if (n < 9) return false;
  const char *ext = base + (n - 9);
  for (int i = 0; i < 9; i++) {
    char a = ext[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (a != ".rec_work"[i]) return false;
  }
  return true;
}

static bool mjpeg_ends_ci(const char *name, const char *suffix) {
  if (!name || !suffix) return false;
  const char *base = strrchr(name, '/');
  base = base ? base + 1 : name;
  size_t n = strlen(base), m = strlen(suffix);
  if (m > n) return false;
  for (size_t i = 0; i < m; i++) {
    char a = base[n - m + i], b = suffix[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<title>ESP32CSI_Vision</title>
<style>
:root{--bg:#0a0e14;--card:#121820;--fg:#e6edf5;--muted:#7d8fa3;--acc:#3d8bfd;--line:#1e2a38;--pad:10px;--face-ok:#2eb87a;--face-det:#6b9bb0;--face-warn:#c4a35a;--r:3px}
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:var(--bg);color:var(--fg);font:13px/1.35 "Segoe UI","Helvetica Neue",sans-serif;overflow:hidden}
body{display:flex;flex-direction:column;padding-bottom:0}
header{flex:0 0 auto;z-index:5;padding:8px 12px;background:rgba(10,14,20,.95);border-bottom:1px solid var(--line);display:flex;flex-wrap:wrap;gap:6px 12px;align-items:center}
header h1{margin:0;font-size:15px;font-weight:650;letter-spacing:.04em}
#meta{color:var(--muted);font-size:11px;word-break:break-word;flex:1 1 160px;font-family:ui-monospace,"Cascadia Mono",Consolas,monospace}
main{flex:1 1 auto;min-height:0;display:grid;grid-template-columns:minmax(0,1.45fr) minmax(280px,340px);gap:10px;padding:10px;align-items:stretch}
@media (max-width:860px){html,body{overflow:auto;height:auto}main{grid-template-columns:1fr;height:auto}}
.view,.panel{background:var(--card);border:1px solid var(--line);border-radius:6px;padding:10px;min-width:0;min-height:0}
.view{display:flex;flex-direction:column;gap:6px;width:100%}
.stage{position:relative;flex:1 1 auto;min-height:180px;background:#000;border-radius:4px;overflow:hidden;line-height:0;isolation:isolate;contain:paint}
.stage img{position:absolute;inset:0;width:100%;height:100%;object-fit:cover;object-position:center;display:block;background:#000;border:0;margin:0}
body.qr-mode .stage img{object-fit:contain}
.stage-note{display:none;position:absolute;left:8px;right:8px;bottom:8px;z-index:3;padding:8px 10px;background:rgba(10,16,24,.9);border:1px solid var(--line);border-radius:4px;color:var(--fg);font-size:12px;line-height:1.4;pointer-events:auto}
.stage-note.on{display:block}
.stage-note a{color:var(--acc);font-weight:650;text-decoration:none}
.links{display:flex;flex-wrap:wrap;gap:6px;flex:0 0 auto}
.links a{color:var(--acc);text-decoration:none;font-size:11px}
.panel{display:flex;flex-direction:column;overflow:hidden;align-self:stretch;max-height:calc(100vh - 56px)}
.tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:8px;flex:0 0 auto}
.tabs button{flex:1 1 auto;min-width:4.5em;padding:7px 8px;font-size:11px;font-weight:650;letter-spacing:.06em;text-transform:uppercase;background:#0a1018;color:var(--muted);border:1px solid var(--line);border-radius:var(--r);cursor:pointer}
.tabs button.on{color:var(--fg);border-color:var(--face-det);background:#15202b}
.tabpane{display:none;flex:1 1 auto;min-height:0;overflow:auto;padding-right:2px}
.tabpane.on{display:block}
.panel h2{margin:0 0 6px;font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.1em}
.row{display:grid;grid-template-columns:1fr auto;gap:4px 8px;align-items:center;margin:6px 0}
.row label{font-size:12px}
.row .val{font-variant-numeric:tabular-nums;color:var(--muted);font-size:11px;min-width:2.2em;text-align:right}
.row .hint,.hint{grid-column:1/-1;color:var(--muted);font-size:10px;margin:2px 0 4px;line-height:1.3}
.row.full{grid-template-columns:1fr}
input[type=range]{width:100%;grid-column:1/-1;accent-color:var(--acc);height:18px;margin:0}
select,button,.field{width:100%;background:#0a1018;color:var(--fg);border:1px solid #2b3b55;border-radius:var(--r);padding:7px 9px;font-size:12px}
.row select{grid-column:1/-1}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:6px}
.btns.single{grid-template-columns:1fr}
button{cursor:pointer;background:var(--acc);border:none;font-weight:600}
button.secondary{background:#1a2433;border:1px solid #2b3b55}
button.capture{background:#2f9e64}
button.record{background:#c0392b}
button.recording{background:#7a1c1c;animation:recpulse 1s infinite}
@keyframes recpulse{50%{filter:brightness(1.25)}}
.timer{font-variant-numeric:tabular-nums;font-size:18px;font-weight:650;letter-spacing:.04em;text-align:center;margin:4px 0}
button:disabled{opacity:.45;cursor:not-allowed}
.live{display:inline-flex;align-items:center;gap:6px;font-size:11px;color:#3dd68c}
.live i{width:7px;height:7px;border-radius:50%;background:#3dd68c;display:inline-block;animation:pulse 1.2s infinite}
@keyframes pulse{50%{opacity:.35}}
.navlink{color:var(--acc);text-decoration:none;font-size:11px;font-weight:600;padding:3px 8px;border:1px solid #2b3b55;border-radius:var(--r)}
.navlink:hover{background:#1a2433}
.navlink.files{background:var(--acc);color:#fff;border-color:#5aa2ff;font-size:12px;font-weight:750;padding:6px 14px;letter-spacing:.04em}
.navlink.files:hover{filter:brightness(1.12);background:#4d97f5}
.navlink.files.wait{opacity:.4;pointer-events:none;filter:none}
.navlink.files.pulse{animation:filespulse 1.2s ease-in-out 4}
@keyframes filespulse{50%{box-shadow:0 0 0 4px rgba(61,139,253,.45)}}
.na{opacity:.5}
#toast{position:fixed;left:50%;bottom:14px;transform:translateX(-50%);background:#102033;border:1px solid #2b3b55;padding:10px 16px;border-radius:12px;display:none;font-size:13px;font-weight:600;line-height:1.4;z-index:40;max-width:min(560px,92vw);white-space:normal;text-align:center}
#toast.ok{background:#143325;border-color:#2eb87a;color:#d4f5e4}
#toast.warn{background:#3a2a10;border-color:#c4a35a;color:#f5e6c8}
.stage-note.warn{background:rgba(58,42,16,.94);border-color:#c4a35a}
.stage-note .files-cta{display:inline-block;margin-left:8px;padding:4px 10px;background:var(--acc);color:#fff;border-radius:4px;font-weight:750}
.save-ov{display:none;position:fixed;inset:0;z-index:30;background:rgba(6,10,16,.72);align-items:center;justify-content:center;padding:16px}
.save-ov.on{display:flex}
.save-card{width:min(420px,94vw);background:#121820;border:1px solid #2b3b55;border-radius:10px;padding:18px 18px 14px;text-align:center}
.save-card h3{margin:0 0 10px;font-size:16px}
.save-bar{height:10px;background:#0a1018;border:1px solid #2b3b55;border-radius:6px;overflow:hidden}
.save-bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .2s}
.save-card .pct{margin:8px 0 4px;font-size:22px;font-weight:750;font-variant-numeric:tabular-nums}
.save-card p{margin:0;color:var(--muted);font-size:12px}
.save-card.ok h3{color:#2eb87a}
.save-card.fail h3{color:#e07070}
.swrow{display:flex;gap:8px;margin:6px 0 8px}
.sw{flex:1;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:8px 10px;background:#0a1018;border:1px solid var(--line);border-radius:var(--r);cursor:pointer;user-select:none}
.sw span{font-size:12px;font-weight:600;letter-spacing:.04em}
.sw input{appearance:none;width:34px;height:18px;border-radius:9px;background:#2a3648;position:relative;outline:none;cursor:pointer;flex:0 0 auto;margin:0;border:none}
.sw input:checked{background:var(--face-ok)}
.sw input::after{content:"";position:absolute;top:2px;left:2px;width:14px;height:14px;border-radius:50%;background:#fff;transition:left .15s}
.sw input:checked::after{left:18px}
.sw input:disabled{opacity:.4;cursor:not-allowed}
.face_status{font-family:ui-monospace,Consolas,monospace;font-size:10px;letter-spacing:.03em;color:var(--face-ok);background:#0a1018;border:1px solid #2a3648;border-radius:var(--r);padding:6px 8px;margin:0 0 8px;line-height:1.35}
.fmtfam{margin:8px 0 4px;font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.08em}
.fmtlist{display:grid;grid-template-columns:1fr 1fr;gap:4px;margin:0 0 8px}
.fmt{display:flex;align-items:center;gap:6px;padding:5px 7px;background:#0a1018;border:1px solid var(--line);border-radius:var(--r);cursor:pointer;user-select:none;font-size:11px}
.fmt input{appearance:auto;width:14px;height:14px;margin:0;accent-color:var(--face-ok);flex:0 0 auto;cursor:pointer}
.fmt span{line-height:1.2}
#btn_face_enroll{background:#1f6b4a}
#wave{width:100%;height:48px;display:block;margin:4px 0;background:#0a1018;border-radius:var(--r)}
.compact .row{margin:4px 0}
</style>
</head>
<body>
<header>
  <h1>ESP32CSI_Vision</h1>
  <span class="live"><i></i>live</span>
  <a id="a_files" class="navlink files" href="#" style="display:none">Files</a>
  <div id="meta">connecting…</div>
</header>
<main>
  <section class="view">
    <div class="stage" id="stage"><img id="stream" alt="live stream"/><div class="stage-note" id="previewNote"></div></div>
    <div class="links">
      <a id="a_stream" href="#" target="_blank">MJPEG</a>
      <a id="a_files2" class="navlink files" href="#" style="display:none">Files</a>
      <a href="/jpg" target="_blank">/jpg</a>
      <a href="/capture" target="_blank">/capture</a>
      <a href="/status" target="_blank">/status</a>
      <a href="/debug" target="_blank">/debug</a>
    </div>
  </section>
  <section class="panel compact">
    <div class="tabs" id="side_tabs">
      <button type="button" data-tab="tab_stream" class="on">Stream</button>
      <button type="button" data-tab="tab_face" id="tabbtn_face">Face</button>
      <button type="button" data-tab="tab_det" id="tabbtn_det">Detect</button>
      <button type="button" data-tab="tab_qr" id="tabbtn_qr">QR</button>
      <button type="button" data-tab="tab_rec" id="tabbtn_rec">Record</button>
      <button type="button" data-tab="tab_sensor">Sensor</button>
    </div>

    <div id="tab_stream" class="tabpane on">
      <h2>Stream</h2>
      <div class="row full"><label>Resolution</label>
        <select id="framesize">
          <option value="0">1920×1088 FHD</option>
          <option value="1" selected>1280×720 HD</option>
          <option value="2">1024×576</option>
          <option value="3">800×448</option>
          <option value="4">640×368</option>
          <option value="5">480×272</option>
          <option value="6">400×224</option>
          <option value="7">320×176</option>
          <option value="8">240×128</option>
          <option value="9">160×96</option>
        </select>
        <div class="hint" id="fs_hint">÷16 sizes · fill frame (cover)</div>
      </div>
      <div class="row"><label>JPEG q</label><span class="val" id="v_quality">35</span>
        <input id="quality" type="range" min="4" max="63" value="35"/>
      </div>
      <div class="row"><label>Skip</label><span class="val" id="v_frameskip">0</span>
        <input id="frameskip" type="range" min="0" max="4" value="0"/>
      </div>
      <div class="btns">
        <button id="btn_reconnect" class="secondary" type="button">Reconnect</button>
        <button id="btn_snap" class="secondary" type="button">Snapshot</button>
      </div>
      <div class="btns single">
        <button id="btn_capture_img" class="capture" type="button">Capture → SD</button>
      </div>
      <div class="hint" id="sd_hint">SD…</div>

      <div id="cv_panel" style="display:none">
      <h2>CV</h2>
      <div class="hint" id="cv_hint">OpenCV-like live tests</div>
      <div class="row full"><label>Mode</label>
        <select id="cv_mode">
          <option value="0">Off</option>
          <option value="7" selected>Edge track</option>
          <option value="1">Blobs</option>
          <option value="2">Mask</option>
          <option value="3">Edges</option>
          <option value="4">Threshold</option>
          <option value="5">Gray</option>
          <option value="6">Blur</option>
        </select>
      </div>
      <div class="row full"><label>Preset</label>
        <select id="cv_preset">
          <option value="6">Dark</option>
          <option value="8" selected>Coins</option>
          <option value="7">Light</option>
          <option value="1">Any</option>
          <option value="2">Red</option>
          <option value="3">Green</option>
          <option value="4">Blue</option>
          <option value="5">Yellow</option>
          <option value="0">Custom</option>
        </select>
      </div>
      <div class="row"><label>H lo</label><span class="val" id="v_cv_h_lo">0</span>
        <input id="cv_h_lo" type="range" min="0" max="179" value="0"/>
      </div>
      <div class="row"><label>H hi</label><span class="val" id="v_cv_h_hi">179</span>
        <input id="cv_h_hi" type="range" min="0" max="179" value="179"/>
      </div>
      <div class="row"><label>S lo</label><span class="val" id="v_cv_s_lo">40</span>
        <input id="cv_s_lo" type="range" min="0" max="255" value="40"/>
      </div>
      <div class="row"><label>S hi</label><span class="val" id="v_cv_s_hi">255</span>
        <input id="cv_s_hi" type="range" min="0" max="255" value="255"/>
      </div>
      <div class="row"><label>V lo</label><span class="val" id="v_cv_v_lo">40</span>
        <input id="cv_v_lo" type="range" min="0" max="255" value="40"/>
      </div>
      <div class="row"><label>V hi</label><span class="val" id="v_cv_v_hi">255</span>
        <input id="cv_v_hi" type="range" min="0" max="255" value="255"/>
      </div>
      <div class="row"><label>Erode</label><span class="val" id="v_cv_erode">0</span>
        <input id="cv_erode" type="range" min="0" max="4" value="0"/>
      </div>
      <div class="row"><label>Dilate</label><span class="val" id="v_cv_dilate">1</span>
        <input id="cv_dilate" type="range" min="0" max="4" value="1"/>
      </div>
      <div class="row"><label>Min area</label><span class="val" id="v_cv_min_area">40</span>
        <input id="cv_min_area" type="range" min="10" max="2000" value="40"/>
      </div>
      <div class="row"><label>Thr</label><span class="val" id="v_cv_thr">110</span>
        <input id="cv_thr" type="range" min="0" max="255" value="110"/>
      </div>
      <div class="row"><label>Edge lo</label><span class="val" id="v_cv_edge_lo">35</span>
        <input id="cv_edge_lo" type="range" min="0" max="255" value="35"/>
      </div>
      <div class="row"><label>Edge hi</label><span class="val" id="v_cv_edge_hi">90</span>
        <input id="cv_edge_hi" type="range" min="0" max="255" value="90"/>
      </div>
      <div class="row"><label>Track</label><span class="val" id="v_cv_track_dist">80</span>
        <input id="cv_track_dist" type="range" min="20" max="200" value="80"/>
      </div>
      </div>
    </div>

    <div id="tab_face" class="tabpane">
      <div id="face_panel">
      <h2>Face ID</h2>
      <div id="face_status" class="face_status">FD idle</div>
      <div class="hint" id="face_hint">SD face.db → PSRAM</div>
      <div class="swrow">
        <label class="sw"><span>DETECT</span><input id="face_detect" type="checkbox"/></label>
        <label class="sw"><span>RECOG</span><input id="face_recog" type="checkbox"/></label>
      </div>
      <div class="row full"><label>Model</label>
        <select id="face_model">
          <option value="0" selected>MSR+MNP · 320×240</option>
          <option value="1">ESPDet 224</option>
          <option value="2">ESPDet 416</option>
        </select>
      </div>
      <div class="row"><label>Match %</label><span class="val" id="v_face_thr">50</span>
        <input id="face_thr" type="range" min="10" max="95" value="50"/>
      </div>
      <div class="row full"><label>Subject</label>
        <input id="face_name" class="field" type="text" maxlength="23" placeholder="NAME" style="text-transform:uppercase;font-family:ui-monospace,Consolas,monospace;letter-spacing:.05em"/>
      </div>
      <div class="btns">
        <button id="btn_face_enroll" type="button">Enroll</button>
        <button id="btn_face_clear" type="button" class="secondary">Clear DB</button>
      </div>
      <div class="row full"><label>Subjects</label>
        <select id="face_roster"><option value="">(empty)</option></select>
      </div>
      <div class="btns single">
        <button id="btn_face_delete" type="button" class="secondary">Delete subject</button>
      </div>
      </div>
    </div>

    <div id="tab_det" class="tabpane">
      <div id="det_panel">
      <h2>Object detect</h2>
      <div id="det_status" class="face_status">OD idle</div>
      <div class="hint">COCO / YOLO26 / cat / dog / hand / pedestrian · models on /models/p4/</div>
      <div class="swrow">
        <label class="sw"><span>DETECT</span><input id="det_en" type="checkbox"/></label>
      </div>
      <div class="row full"><label>Model</label>
        <select id="det_model">
          <option value="0" selected>YOLO11n · 640 · COCO</option>
          <option value="1">YOLO11n · 320 · COCO</option>
          <option value="2">Pico · pedestrian</option>
          <option value="3">ESPDet · cat 224</option>
          <option value="4">ESPDet · cat 416</option>
          <option value="5">ESPDet · dog 224</option>
          <option value="6">ESPDet · dog 416</option>
          <option value="7">ESPDet · hand 224</option>
          <option value="8">YOLO26n · 640 · COCO</option>
          <option value="9">YOLO26n · 512 · COCO</option>
        </select>
      </div>
      <div class="row"><label>Score %</label><span class="val" id="v_det_thr">25</span>
        <input id="det_thr" type="range" min="5" max="95" value="25"/>
      </div>
      <div class="row full"><label>Last boxes</label>
        <input id="det_summary" class="field" type="text" readonly placeholder="label score @ x,y wxh"/>
      </div>
      </div>
    </div>

    <div id="tab_qr" class="tabpane">
      <div id="qr_panel">
      <h2>QR / barcode</h2>
      <div id="qr_status" class="face_status">QR idle</div>
      <div class="hint">Toggle formats below · settings saved to SD / FFat / flash · half-res async</div>
      <div class="swrow">
        <label class="sw"><span>SCAN</span><input id="qr_en" type="checkbox"/></label>
      </div>
      <div class="row full"><label>Type</label>
        <input id="qr_fmt" class="field" type="text" readonly placeholder="(none yet)"/>
      </div>
      <div class="row full"><label>Last payload</label>
        <input id="qr_payload" class="field" type="text" readonly placeholder="(none yet)"/>
      </div>
      <div class="btns single">
        <button id="btn_qr_copy" type="button" class="secondary">Copy payload</button>
      </div>
      <div class="fmtfam">Matrix</div>
      <div class="fmtlist" id="qr_fmts_matrix"></div>
      <div class="fmtfam">1D</div>
      <div class="fmtlist" id="qr_fmts_linear"></div>
      <div class="fmtfam">GS1</div>
      <div class="fmtlist" id="qr_fmts_gs1"></div>
      </div>
    </div>

    <div id="tab_rec" class="tabpane">
      <h2>Record</h2>
      <canvas id="wave" width="640" height="48"></canvas>
      <div class="btns single">
        <button id="btn_audio_live" type="button" class="secondary">Play live audio</button>
      </div>
      <div class="row"><label>Mic</label><span class="val" id="v_mic_gain">55</span>
        <input id="mic_gain" type="range" min="0" max="100" value="55"/>
      </div>
      <div class="hint" id="mic_hint">Mic…</div>
      <div class="timer" id="rec_timer">00:00</div>
      <div class="save-bar" id="rec_bar_wrap" style="display:none;margin:8px 0"><i id="rec_bar"></i></div>
      <div class="hint" id="rec_save_pct" style="display:none;text-align:center;font-weight:700"></div>
      <div class="btns single">
        <button id="btn_record" class="record" type="button" disabled>Record</button>
      </div>
      <div class="hint" id="vid_hint">Video…</div>
    </div>

    <div id="tab_sensor" class="tabpane">
      <h2>OV5647</h2>
      <div class="hint" id="sensor_hint">Flip updates ISP Bayer order</div>
      <div class="swrow">
        <label class="sw"><span>H-MIR</span><input id="hmirror" type="checkbox"/></label>
        <label class="sw"><span>V-FLIP</span><input id="vflip" type="checkbox"/></label>
      </div>
      <div class="swrow">
        <label class="sw"><span>SMART AE</span><input id="smart_ae" type="checkbox"/></label>
      </div>
      <div class="hint" id="smart_ae_hint">IPA AGC on RAW sensors; Smart AE is RGB565 fallback</div>
      <div class="row"><label>EV bias</label><span class="val" id="v_smart_ae_ev">0</span>
        <input id="smart_ae_ev" type="range" min="-4" max="4" value="0"/>
      </div>
      <div class="swrow">
        <label class="sw"><span>AEC</span><input id="aec" type="checkbox" checked/></label>
        <label class="sw"><span>AGC</span><input id="agc" type="checkbox" checked/></label>
      </div>
      <div class="row"><label>Exposure</label><span class="val" id="v_aec_value">100</span>
        <input id="aec_value" type="range" min="4" max="980" value="100"/>
      </div>
      <div class="row"><label>Gain</label><span class="val" id="v_agc_gain">16</span>
        <input id="agc_gain" type="range" min="0" max="1023" value="16"/>
      </div>
      <div class="row"><label>Gain ceil</label><span class="val" id="v_gainceiling">248</span>
        <input id="gainceiling" type="range" min="16" max="1023" value="248"/>
      </div>
      <div class="swrow">
        <label class="sw"><span>PATTERN</span><input id="colorbar" type="checkbox"/></label>
      </div>
    </div>
  </section>
</main>
<div id="toast"></div>
<div id="saveOverlay" class="save-ov" aria-live="polite">
  <div class="save-card" id="saveCard">
    <h3 id="saveTitle">Saving video…</h3>
    <div class="save-bar"><i id="saveBar"></i></div>
    <div class="pct" id="savePct">0%</div>
    <p id="saveMsg">Writing MP4 — stay here until it finishes</p>
  </div>
</div>
<script>
const stream=document.getElementById('stream');
const stage=document.getElementById('stage');
const basePort=location.port?parseInt(location.port,10):80;
const streamPort=basePort+1;
const audioPort=basePort+4;
const streamUrl=location.protocol+'//'+location.hostname+':'+streamPort+'/stream';
const audioUrl=location.protocol+'//'+location.hostname+':'+audioPort+'/audio.pcm';
document.getElementById('a_stream').href=streamUrl;
function reconnectStream(){stream.src=streamUrl+'?ts='+Date.now()}
function clearPreview(){ stream.removeAttribute('src'); stream.src=''; }
let audioCtx=null, audioPlayOn=false, audioAbort=null, audioNextT=0;
async function toggleLiveAudio(){
  const btn=document.getElementById('btn_audio_live');
  if(audioPlayOn){
    audioPlayOn=false;
    try{if(audioAbort) audioAbort.abort()}catch(e){}
    audioAbort=null;
    try{if(audioCtx) await audioCtx.close()}catch(e){}
    audioCtx=null;
    if(btn) btn.textContent='Play live audio';
    return;
  }
  const AC=window.AudioContext||window.webkitAudioContext;
  if(!AC){toast('Web Audio not supported');return}
  audioCtx=new AC({latencyHint:'interactive'});
  await audioCtx.resume();
  audioPlayOn=true;
  audioAbort=new AbortController();
  audioNextT=audioCtx.currentTime+0.06;
  if(btn) btn.textContent='Stop live audio';
  try{
    const res=await fetch(audioUrl+'?ts='+Date.now(),{cache:'no-store',signal:audioAbort.signal});
    if(!res.ok || !res.body) throw new Error('audio '+res.status);
    const reader=res.body.getReader();
    const srcRate=16000;
    let leftover=new Uint8Array(0);
    while(audioPlayOn){
      const {done,value}=await reader.read();
      if(done) break;
      if(!value || !value.length) continue;
      const merged=new Uint8Array(leftover.length+value.length);
      merged.set(leftover,0); merged.set(value,leftover.length);
      const even=merged.length&~1;
      leftover=merged.slice(even);
      if(even<2) continue;
      const pcm=new Int16Array(merged.buffer,merged.byteOffset,even/2);
      const ratio=audioCtx.sampleRate/srcRate;
      const frames=Math.max(1, Math.floor(pcm.length*ratio));
      const buf=audioCtx.createBuffer(1,frames,audioCtx.sampleRate);
      const dst=buf.getChannelData(0);
      if(Math.abs(ratio-1)<0.001){
        for(let i=0;i<pcm.length && i<frames;i++) dst[i]=pcm[i]/32768;
      }else{
        for(let i=0;i<frames;i++){
          const x=i/ratio;
          const i0=Math.min(pcm.length-1, x|0);
          const i1=Math.min(pcm.length-1, i0+1);
          const f=x-i0;
          dst[i]=((pcm[i0]*(1-f)+pcm[i1]*f)/32768);
        }
      }
      const now=audioCtx.currentTime;
      if(audioNextT<now+0.02) audioNextT=now+0.04;
      if(audioNextT>now+0.12) continue;
      const src=audioCtx.createBufferSource();
      src.buffer=buf;
      src.connect(audioCtx.destination);
      src.start(audioNextT);
      audioNextT+=buf.duration;
    }
  }catch(e){
    if(e && e.name==='AbortError') return;
    toast('live audio failed');
  }finally{
    if(audioPlayOn) toggleLiveAudio();
  }
}
document.getElementById('btn_audio_live').addEventListener('click',()=>{toggleLiveAudio()});
let applying=false, debounceTimers={}, faceBusy=false, statusToken=0;

function toast(m,ms,kind){const t=document.getElementById('toast');t.textContent=m;t.className=kind||'';t.style.display='block';clearTimeout(t._t);t._t=setTimeout(()=>{t.style.display='none';t.className='';},ms||2200)}
function escHtml(s){return String(s||'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function filesUrl(){const fp=(window.CAM_FILES_PORT|0);return fp>0?(location.protocol+'//'+location.hostname+':'+fp+'/'):'';}
function setFilesWait(on){
  document.querySelectorAll('#a_files,#a_files2').forEach(a=>{
    a.classList.toggle('wait', !!on);
    a.title=on?'Wait until the video finishes saving':'Open Files';
  });
}
function setFilesPulse(on){
  document.querySelectorAll('#a_files,#a_files2').forEach(a=>a.classList.toggle('pulse', !!on));
}
let waitingSave=false, saveWasFinal=false;
function showSaveOverlay(pct, title, msg, state){
  const ov=document.getElementById('saveOverlay');
  const card=document.getElementById('saveCard');
  const bar=document.getElementById('saveBar');
  const p=document.getElementById('savePct');
  if(title) document.getElementById('saveTitle').textContent=title;
  if(msg) document.getElementById('saveMsg').textContent=msg;
  const n=Math.max(0, Math.min(100, pct|0));
  if(bar) bar.style.width=n+'%';
  if(p) p.textContent=n+'%';
  if(card){ card.classList.toggle('ok', state==='ok'); card.classList.toggle('fail', state==='fail'); }
  if(ov) ov.classList.add('on');
  const rb=document.getElementById('rec_bar_wrap'), ri=document.getElementById('rec_bar'), rp=document.getElementById('rec_save_pct');
  if(rb) rb.style.display='block';
  if(ri) ri.style.width=n+'%';
  if(rp){ rp.style.display='block'; rp.textContent='Saving MP4  '+n+'%'; }
}
function hideSaveOverlay(delay){
  const go=()=>{
    const ov=document.getElementById('saveOverlay'); if(ov) ov.classList.remove('on');
    const rb=document.getElementById('rec_bar_wrap'), rp=document.getElementById('rec_save_pct');
    if(rb) rb.style.display='none';
    if(rp) rp.style.display='none';
  };
  if(delay) setTimeout(go, delay); else go();
}
function onSaveProgress(s){
  if(s.finalizing){
    waitingSave=true; saveWasFinal=true;
    setFilesWait(true); setFilesPulse(false);
    showSaveOverlay(s.mux_pct||5, 'Saving video…', 'Muxing H.264 → MP4. Do not open Files yet.', '');
    const btn=document.getElementById('btn_record');
    if(btn){ btn.disabled=true; btn.textContent='Saving…'; }
    return true;
  }
  if(waitingSave || saveWasFinal){
    waitingSave=false; saveWasFinal=false;
    setFilesWait(false);
    const path=s.last_video||'MP4';
    if(s.rec_save_ok===1){
      showSaveOverlay(100, 'Video saved', path+' is ready in Files', 'ok');
      toast('Video saved — '+path, 5000, 'ok');
      setFilesPulse(true);
      hideSaveOverlay(2800);
    }else if(s.rec_save_ok===-1){
      showSaveOverlay(100, 'Save failed', 'Recording did not become an MP4. Try Record again.', 'fail');
      toast('Video save failed', 5000, 'warn');
      hideSaveOverlay(4000);
    }else{
      hideSaveOverlay(0);
    }
    const btn=document.getElementById('btn_record');
    if(btn && s.video_ok){ btn.disabled=false; btn.textContent='Record'; }
  }
  return false;
}

async function control(varName,val){
  applying=true;
  try{
    const ctrl=new AbortController();
    const timer=setTimeout(()=>ctrl.abort(),4000);
    const r=await fetch('/control?var='+encodeURIComponent(varName)+'&val='+encodeURIComponent(val)+'&_='+Date.now(),{cache:'no-store',signal:ctrl.signal});
    clearTimeout(timer);
    if(!r.ok){toast('failed: '+varName);return}
    if(varName!=='mic_gain' && varName!=='det_en' && varName!=='face_detect') toast(varName+'='+val);
    // Mic gain updates live via /audio waveform — skip full status refresh.
    if(varName!=='mic_gain') refreshMetaOnly();
  }catch(e){toast(e.name==='AbortError'?'timeout':'network error')}
  finally{applying=false}
}

function bindRange(id,varName){
  const el=document.getElementById(id), lab=document.getElementById('v_'+id);
  if(!el) return;
  const sync=()=>{if(lab)lab.textContent=el.value};
  el.addEventListener('input',()=>{
    sync();
    clearTimeout(debounceTimers[id]);
    debounceTimers[id]=setTimeout(async()=>{
      if(varName==='aec_value'){
        const a=document.getElementById('aec'); if(a){ a.checked=false; }
        await control('aec','0');
      }
      if(varName==='agc_gain'){
        const a=document.getElementById('agc'); if(a){ a.checked=false; }
        await control('agc','0');
      }
      await control(varName,el.value);
    }, varName==='mic_gain' ? 40 : 80);
  });
  sync();
}
function bindSelect(id,varName){
  const el=document.getElementById(id); if(!el) return;
  el.addEventListener('change',e=>control(varName,e.target.value));
}
function bindSwitch(id,varName){
  const el=document.getElementById(id); if(!el) return;
  el.addEventListener('change',()=>control(varName, el.checked?'1':'0'));
}
function faceOn(el){ return !!(el && (el.type==='checkbox'?el.checked:el.value==='1')); }
function setFaceOn(el,on){ if(!el) return; if(el.type==='checkbox') el.checked=!!on; else el.value=on?'1':'0'; }
function showTab(id){
  document.querySelectorAll('.tabpane').forEach(p=>p.classList.toggle('on',p.id===id));
  document.querySelectorAll('#side_tabs button').forEach(b=>b.classList.toggle('on',b.dataset.tab===id));
}
document.getElementById('side_tabs').addEventListener('click',e=>{
  const b=e.target.closest('button[data-tab]'); if(!b) return;
  showTab(b.dataset.tab);
});
bindRange('quality','quality');
bindRange('frameskip','frameskip');
bindRange('aec_value','aec_value');
bindRange('agc_gain','agc_gain');
bindRange('gainceiling','gainceiling');
bindRange('smart_ae_ev','smart_ae_ev');
bindRange('mic_gain','mic_gain');
document.getElementById('framesize').addEventListener('change',async e=>{
  stream.removeAttribute('src');
  stream.src='';
  await control('framesize',e.target.value);
  try{
    const s=await (await fetch('/status?_='+Date.now(),{cache:'no-store'})).json();
    if(s.out_w&&s.out_h) stream.dataset.outRes=String(s.out_w)+'x'+String(s.out_h);
  }catch(err){}
  reconnectStream();
});
bindSwitch('hmirror','hmirror');
bindSwitch('vflip','vflip');
bindSwitch('smart_ae','smart_ae');
bindSwitch('aec','aec');
bindSwitch('agc','agc');
bindSwitch('colorbar','colorbar');
function syncSmartAeUi(s){
  const on=!!(s&&s.smart_ae);
  ['aec','agc','aec_value','agc_gain'].forEach(id=>{
    const el=document.getElementById(id); if(el) el.disabled=on;
  });
  const hint=document.getElementById('smart_ae_hint');
  if(hint){
    if(on){
      hint.textContent='Smart AE · meter '+(s.smart_ae_meter!=null?Number(s.smart_ae_meter).toFixed(0):'—')+
        ' · hi '+((s.smart_ae_hi!=null)?(Number(s.smart_ae_hi)*100).toFixed(0)+'%':'—')+
        ' · exp '+(s.aec_value||'—')+' · gain '+(s.agc_gain||'—')+
        ' · '+(s.smart_ae_ms||0)+'ms';
    }else{
      hint.textContent=s&&s.isp_ready
        ? ('IPA AGC · luma '+(s.isp_luma!=null?Number(s.isp_luma).toFixed(0):'—')+
           ' · env '+(s.isp_env!=null?Number(s.isp_env).toFixed(0):'—')+
           ' · EV '+(s.smart_ae_ev||0))
        : 'Software AE fallback (no ISP stats on this path)';
    }
  }
}
bindSelect('cv_mode','cv_mode');
bindSelect('cv_preset','cv_preset');
function syncFaceToggles(){
  const fd=document.getElementById('face_detect');
  const fr=document.getElementById('face_recog');
  if(!fd||!fr) return;
  if(!faceOn(fd)){ setFaceOn(fr,false); fr.disabled=true; }
  else fr.disabled=false;
}
document.getElementById('face_model').addEventListener('change',e=>applyFaceAndRes('face_model',e.target.value));
document.getElementById('face_detect').addEventListener('change',async e=>{
  faceBusy=true;
  try{
    const on=faceOn(e.target);
    if(!on){
      setFaceOn(document.getElementById('face_recog'),false);
      await applyFaceAndRes('face_detect','0');
      await control('face_recog','0');
    }else{
      await applyFaceAndRes('face_detect','1');
    }
    syncFaceToggles();
  }finally{ faceBusy=false; }
});
document.getElementById('qr_en').addEventListener('change',async e=>{
  await control('qr_en', faceOn(e.target)?'1':'0');
});
document.getElementById('det_en').addEventListener('change',async e=>{
  await control('det_en', faceOn(e.target)?'1':'0');
});
bindSelect('det_model','det_model');
bindRange('det_thr','det_thr');
document.getElementById('btn_qr_copy').onclick=async()=>{
  const p=document.getElementById('qr_payload');
  const t=(p && p.value)||'';
  if(!t){ toast('no payload'); return; }
  try{ await navigator.clipboard.writeText(t); toast('copied'); }
  catch(err){ toast('copy failed'); }
};
document.getElementById('face_recog').addEventListener('change',async e=>{
  faceBusy=true;
  try{
    if(faceOn(e.target)){
      setFaceOn(document.getElementById('face_detect'),true);
      syncFaceToggles();
      await control('face_detect','1');
      await applyFaceAndRes('face_recog','1');
      setFaceOn(document.getElementById('face_detect'),true);
      setFaceOn(document.getElementById('face_recog'),true);
    }else{
      await applyFaceAndRes('face_recog','0');
    }
    syncFaceToggles();
  }finally{ faceBusy=false; }
});
syncFaceToggles();
bindRange('face_thr','face_thr');
['cv_h_lo','cv_h_hi','cv_s_lo','cv_s_hi','cv_v_lo','cv_v_hi','cv_erode','cv_dilate','cv_min_area','cv_thr','cv_edge_lo','cv_edge_hi','cv_track_dist'].forEach(id=>bindRange(id,id));
document.getElementById('btn_face_enroll').onclick=async()=>{
  const name=(document.getElementById('face_name').value||'').trim();
  if(!name){ toast('enter a name first'); return; }
  faceBusy=true;
  try{
    const modelEl=document.getElementById('face_model');
    if(modelEl && modelEl.value!=='0'){
      modelEl.value='0';
      await applyFaceAndRes('face_model','0');
    }
    setFaceOn(document.getElementById('face_detect'),true);
    // Keep RECOG as-is — firmware skips FR only during the enroll window.
    syncFaceToggles();
    await control('face_detect','1');
    await control('face_enroll_name', name);
    await control('face_enroll', 1);
    showTab('tab_face');
    toast('ENROLL '+name.toUpperCase()+' — hold steady (5 confirms → 1 ID)');
  }finally{ faceBusy=false; }
};
document.getElementById('btn_face_clear').onclick=()=>{ if(confirm('Clear entire face database?')) control('face_clear',1); };
document.getElementById('btn_face_delete').onclick=()=>{
  const sel=document.getElementById('face_roster');
  const name=sel && sel.value;
  if(!name){ toast('select a subject'); return; }
  if(confirm('Delete subject "'+name+'" (all samples)?')) control('face_delete_name', name);
};
function fillFaceRoster(roster){
  const sel=document.getElementById('face_roster'); if(!sel) return;
  const prev=sel.value;
  sel.innerHTML='';
  const parts=(roster||'').split('|').filter(Boolean);
  if(!parts.length){ const o=document.createElement('option'); o.value=''; o.textContent='(empty)'; sel.appendChild(o); return; }
  parts.forEach(p=>{
    // name#id#count  (legacy id:name still accepted)
    let name='', id='', count='1';
    if(p.indexOf('#')>=0){
      const bits=p.split('#');
      name=bits[0]||''; id=bits[1]||''; count=bits[2]||'1';
    }else{
      const i=p.indexOf(':');
      id=i>=0?p.slice(0,i):p;
      name=i>=0?p.slice(i+1):('ID'+id);
    }
    const o=document.createElement('option');
    o.value=name;
    const c=parseInt(count,10)||1;
    o.textContent=name.toUpperCase()+'  ·  ID '+id+(c>1?('  ·  '+c+' feats'):'');
    sel.appendChild(o);
  });
  if(prev) sel.value=prev;
}
function faceStatusText(s){
  const db=(s.face_db||'/sdcard/face/face.db').replace(/^\/sdcard\//,'SD:/');
  let st=db+' → PSRAM  ·  DET '+(s.face_n||0)+'  ·  '+(s.face_ms||0)+'ms  ·  DB '+(s.face_feats||0);
  if(s.face_enroll_ok===2) st+='  ·  ENROLL '+(s.face_enroll_got||0)+'/'+(s.face_enroll_need||5);
  else if(s.face_detect===0) st+='  ·  DET OFF';
  else if(s.face_recog===0) st+='  ·  REC OFF';
  else st+='  ·  REC ON';
  if(s.face_enroll_ok===1) st+='  ·  STORED #'+(s.face_enroll_id||'?');
  else if(s.face_enroll_ok===-1) st+='  ·  ENROLL FAIL (SD write)';
  else if(s.face_enroll_ok===-2) st+='  ·  NO MFN';
  else if(s.face_enroll_ok===-3) st+='  ·  NEED MSR+MNP';
  return st;
}
function setFaceStatus(s){
  const el=document.getElementById('face_status');
  if(el) el.textContent=faceStatusText(s);
}
function qrStatusText(s){
  const ms=(s.qr_ms==null)?0:Number(s.qr_ms);
  let st='BC '+(s.qr_n||0)+'  ·  '+(ms<0?'…':(ms+'ms'));
  if(s.qr_en===0) st+='  ·  SCAN OFF';
  else st+='  ·  SCAN ON';
  if(s.qr_fmt) st+='  ·  '+String(s.qr_fmt);
  if(s.qr_payload) st+='  ·  '+String(s.qr_payload).slice(0,32);
  return st;
}
function setQrStatus(s){
  const el=document.getElementById('qr_status');
  if(el) el.textContent=qrStatusText(s);
  const p=document.getElementById('qr_payload');
  if(p && s.qr_payload!=null && document.activeElement!==p) p.value=String(s.qr_payload||'');
  const f=document.getElementById('qr_fmt');
  if(f && s.qr_fmt!=null && document.activeElement!==f) f.value=String(s.qr_fmt||'');
  if(s.qr_fmts!=null) applyQrFmtChecks(Number(s.qr_fmts)||0);
}
function detStatusText(s){
  let st='OD '+(s.det_n||0)+'  ·  '+(s.det_ms||0)+'ms';
  if(s.det_en===0) st+='  ·  DET OFF';
  else st+='  ·  DET ON';
  if(s.det_summary) st+='  ·  '+String(s.det_summary).slice(0,96);
  return st;
}
function setDetStatus(s){
  const el=document.getElementById('det_status');
  if(el) el.textContent=detStatusText(s);
  const sum=document.getElementById('det_summary');
  if(sum && s.det_summary!=null && document.activeElement!==sum) sum.value=String(s.det_summary||'');
  const thr=document.getElementById('det_thr');
  const vthr=document.getElementById('v_det_thr');
  if(thr && s.det_thr!=null && document.activeElement!==thr){ thr.value=String(s.det_thr); if(vthr) vthr.textContent=String(s.det_thr); }
  const dm=document.getElementById('det_model');
  if(dm && s.det_model!=null && document.activeElement!==dm) dm.value=String(s.det_model);
}

/* ZXing BarcodeFormat bit flags (must match firmware). */
const QR_FMT_ITEMS=[
  {bit:1<<13, fam:'matrix', label:'QR Code'},
  {bit:1<<16, fam:'matrix', label:'Micro QR'},
  {bit:1<<17, fam:'matrix', label:'rMQR'},
  {bit:1<<0,  fam:'matrix', label:'Aztec'},
  {bit:1<<12, fam:'matrix', label:'PDF417'},
  {bit:1<<4,  fam:'linear', label:'Code 128'},
  {bit:1<<2,  fam:'linear', label:'Code 39'},
  {bit:1<<3,  fam:'linear', label:'Code 93'},
  {bit:1<<1,  fam:'linear', label:'Codabar'},
  {bit:1<<8,  fam:'linear', label:'EAN-8'},
  {bit:1<<9,  fam:'linear', label:'EAN-13'},
  {bit:1<<14, fam:'linear', label:'UPC-A'},
  {bit:1<<15, fam:'linear', label:'UPC-E'},
  {bit:1<<10, fam:'linear', label:'ITF'},
  {bit:1<<5,  fam:'gs1', label:'DataBar'},
  {bit:1<<6,  fam:'gs1', label:'DataBar Exp'},
  {bit:1<<19, fam:'gs1', label:'DataBar Ltd'},
];
const QR_FMT_ALL=QR_FMT_ITEMS.reduce((a,x)=>a|x.bit,0);
let qrFmtApplying=false;
function buildQrFmtLists(){
  const hosts={matrix:'qr_fmts_matrix',linear:'qr_fmts_linear',gs1:'qr_fmts_gs1'};
  for(const k of Object.keys(hosts)){const el=document.getElementById(hosts[k]); if(el) el.innerHTML='';}
  for(const it of QR_FMT_ITEMS){
    const host=document.getElementById(hosts[it.fam]);
    if(!host) continue;
    const lab=document.createElement('label');
    lab.className='fmt';
    lab.innerHTML='<input type="checkbox" data-bit="'+it.bit+'" checked/><span>'+it.label+'</span>';
    const inp=lab.querySelector('input');
    inp.addEventListener('change', onQrFmtChange);
    host.appendChild(lab);
  }
}
function readQrFmtMask(){
  let m=0;
  document.querySelectorAll('#qr_panel input[data-bit]').forEach(inp=>{
    if(inp.checked) m|=Number(inp.getAttribute('data-bit'))||0;
  });
  return m||QR_FMT_ALL;
}
function applyQrFmtChecks(mask){
  if(qrFmtApplying) return;
  qrFmtApplying=true;
  const m=(mask>>>0)||QR_FMT_ALL;
  document.querySelectorAll('#qr_panel input[data-bit]').forEach(inp=>{
    if(document.activeElement===inp) return;
    const bit=Number(inp.getAttribute('data-bit'))||0;
    inp.checked=!!(m&bit);
  });
  qrFmtApplying=false;
}
async function onQrFmtChange(){
  if(qrFmtApplying) return;
  const m=readQrFmtMask();
  await control('qr_fmts', String(m>>>0));
}

function syncFaceResLock(s){
  const fs=document.getElementById('framesize');
  const locked=!!(s && (s.face_lock || ((s.face_detect||0)|(s.face_recog||0))));
  if(fs) fs.disabled=locked;
  const fsh=document.getElementById('fs_hint');
  if(fsh && s){
    if(locked) fsh.textContent='FACE LOCK '+s.out_w+'×'+s.out_h+' · sensor '+s.native_w+'×'+s.native_h+' (model input forced)';
    else fsh.textContent='Stream '+s.out_w+'×'+s.out_h+' · sensor '+s.native_w+'×'+s.native_h+' · cover-fill frame';
  }
  return locked;
}
async function applyFaceAndRes(varName,val){
  const before=stream.dataset.faceLock||'0';
  await control(varName,val);
  try{
    const s=await (await fetch('/status?_='+Date.now(),{cache:'no-store'})).json();
    const locked=syncFaceResLock(s);
    stream.dataset.faceLock=locked?'1':'0';
    const next=String(s.out_w)+'x'+String(s.out_h);
    if(next!==(stream.dataset.outRes||'') || before!==stream.dataset.faceLock){
      stream.dataset.outRes=next;
      stream.removeAttribute('src'); stream.src='';
      reconnectStream();
    }
    if(s.framesize!=null){
      const fs=document.getElementById('framesize');
      if(fs && document.activeElement!==fs) fs.value=String(s.framesize);
    }
  }catch(e){}
}
document.getElementById('btn_reconnect').onclick=reconnectStream;
stream.onerror=()=>{setTimeout(reconnectStream,500)};
// Auto-reconnect if MJPEG stops advancing (common after a stalled TCP send).
let lastSentWatch=-1, stallTicks=0;
setInterval(async()=>{
  if(document.hidden) return;
  try{
    const r=await fetch('/status?_='+Date.now(),{cache:'no-store'});
    const s=await r.json();
    if(typeof s.sent==='number' && s.sent!==lastSentWatch){ lastSentWatch=s.sent; stallTicks=0; }
    else if(++stallTicks>=4){ stallTicks=0; toast('stream stalled — reconnect'); reconnectStream(); }
  }catch(e){}
},2000);
document.getElementById('btn_snap').onclick=()=>{
  // Same-origin snapshot on control port (not blocked by MJPEG).
  window.open('/capture?ts='+Date.now(),'_blank');
};
document.getElementById('btn_capture_img').onclick=async()=>{
  const btn=document.getElementById('btn_capture_img');
  btn.disabled=true;
  try{
    const ctrl=new AbortController();
    const timer=setTimeout(()=>ctrl.abort(),15000);
    const r=await fetch('/capture_img?_='+Date.now(),{cache:'no-store',signal:ctrl.signal});
    clearTimeout(timer);
    const j=await r.json().catch(()=>({}));
    if(!r.ok){toast(j.error||('save failed '+r.status));return}
    toast('saved '+ (j.path||'OK'));
    refreshMetaOnly();
  }catch(e){toast(e.name==='AbortError'?'save timeout':'capture error')}
  finally{btn.disabled=false}
};

const waveEl=document.getElementById('wave');
const wctx=waveEl?waveEl.getContext('2d'):null;
function drawWave(bins,rms,peak){
  if(!wctx||!waveEl) return;
  const W=waveEl.width,H=waveEl.height;
  wctx.clearRect(0,0,W,H);
  wctx.fillStyle='#0b1220'; wctx.fillRect(0,0,W,H);
  const n=(bins&&bins.length)?bins.length:0;
  if(!n){ wctx.fillStyle='#334'; wctx.fillRect(0,H/2-1,W,2); return; }
  // Levels already include mic gain — draw 1:1 so the slider feels direct.
  const gap=1, bw=Math.max(1,(W/n)-gap);
  for(let i=0;i<n;i++){
    const v=Math.max(0,Math.min(100,bins[i]|0))/100;
    const h=Math.max(2,v*(H-8));
    const x=i*(bw+gap);
    const g=Math.floor(80+v*140), b=Math.floor(140+v*80);
    wctx.fillStyle='rgb(40,'+g+','+b+')';
    wctx.fillRect(x,(H-h)/2,bw,h);
  }
  wctx.fillStyle='#8ab'; wctx.font='11px monospace';
  wctx.fillText('rms '+(rms*100).toFixed(0)+'%  peak '+(peak*100).toFixed(0)+'%',8,14);
}
async function tickWave(){
  try{
    const r=await fetch('/audio?_='+Date.now(),{cache:'no-store'});
    if(!r.ok){
      document.getElementById('mic_hint').textContent='Mic waveform disabled';
      const mg=document.getElementById('mic_gain'); if(mg) mg.disabled=true;
      return;
    }
    const j=await r.json();
    const mh=document.getElementById('mic_hint');
    const mg=document.getElementById('mic_gain');
    if(mg) mg.disabled=!j.ok;
    if(mh) mh.textContent=j.ok
      ? ('Mic live @ '+(j.rate||16000)+' Hz · gain '+(j.gain!=null?j.gain: '?')+'% · Play live audio (~40 ms packets) · AAC in MP4 on Record')
      : 'Mic not ready';
    if(j.ok && j.gain!=null && !applying){
      const el=document.getElementById('mic_gain'), lab=document.getElementById('v_mic_gain');
      if(el && document.activeElement!==el){ el.value=String(j.gain); if(lab) lab.textContent=String(j.gain); }
    }
    drawWave(j.wave||[], j.rms||0, j.peak||0);
  }catch(e){}
}
setInterval(()=>{
  if(document.hidden) return;
  const recOn=!!document.getElementById('tab_rec')?.classList.contains('on');
  if(!recOn && !audioPlayOn) return;
  tickWave();
},250);

let recording=false;
let recBusy=false;
function fmtMs(ms){
  ms=Math.max(0,ms|0);
  const s=Math.floor(ms/1000), m=Math.floor(s/60), r=s%60;
  return String(m).padStart(2,'0')+':'+String(r).padStart(2,'0');
}
function setRecUi(on, elapsed){
  recording=!!on;
  const btn=document.getElementById('btn_record');
  const t=document.getElementById('rec_timer');
  if(t) t.textContent=fmtMs(elapsed||0);
  if(!btn) return;
  btn.classList.toggle('recording', recording);
  btn.textContent=recording?'Stop':'Record';
}
document.getElementById('btn_record').onclick=async()=>{
  if(recBusy) return;
  const btn=document.getElementById('btn_record');
  const wasRec=recording;
  recBusy=true;
  btn.disabled=true;
  // Immediate UI feedback — do not wait for SD/mux.
  if(wasRec){ setRecUi(false,0); toast('Stopping…'); }
  else { setRecUi(true,0); waitingSave=false; saveWasFinal=false; hideSaveOverlay(0); setFilesWait(false); setFilesPulse(false); toast('Starting record…'); }
  try{
    const url=wasRec?'/record/stop':'/record/start';
    const ctrl=new AbortController();
    const timer=setTimeout(()=>ctrl.abort(),15000);
    const r=await fetch(url+'?_='+Date.now(),{cache:'no-store',signal:ctrl.signal});
    clearTimeout(timer);
    const j=await r.json().catch(()=>({}));
    if(!r.ok){
      setRecUi(wasRec,0); // revert
      toast(j.error||('record failed '+r.status));
      return;
    }
    setRecUi(!!j.recording, j.elapsed_ms||0);
    if(j.recording) toast('Recording '+ (j.path||''), 2200, 'ok');
    else if(j.finalizing){ onSaveProgress({finalizing:1,mux_pct:5,video_ok:1}); toast('Saving video…', 2500); }
    else if(j.path) toast('Video saved — '+j.path, 5000, 'ok');
    else toast('saved OK', 2500, 'ok');
  }catch(e){
    setRecUi(false,0);
    toast(e.name==='AbortError'?'record timeout — is UI on port 80 (camera), not file manager?':'record error');
  }
  finally{
    recBusy=false;
    // Re-enable if video record is available (status refresh will refine).
    btn.disabled=false;
    refreshMetaOnly();
  }
};

let lastDetOn=undefined, lastFaceOn=undefined;
function applyPreviewNote(s){
  const el=document.getElementById('previewNote');
  const t=(s && s.preview_note)?String(s.preview_note).trim():'';
  if(el){
    if(t){
      el.classList.add('on','warn');
      const u=filesUrl();
      el.innerHTML=escHtml(t)+(u?' <a class="files-cta" href="'+u+'">Open Files</a>':'');
      setFilesPulse(true);
    }else{
      el.classList.remove('on','warn');
      el.innerHTML='';
    }
  }
  const detOn=!!(s && s.det_ok && s.det_en);
  const faceOn=!!(s && s.face_ok && s.face_detect);
  if(t && ((detOn && lastDetOn!==true) || (faceOn && lastFaceOn!==true))){
    toast(t, 6000, 'warn');
  }
  lastDetOn=detOn;
  lastFaceOn=faceOn;
}
function fillForm(s){
  // Do not dispatch range 'input' here — that re-fires /control and can fight face toggles.
  const set=(id,v)=>{
    const el=document.getElementById(id); if(!el||v===undefined||v===null)return;
    if(el.type==='checkbox'){ el.checked=!!(+v); return; }
    el.value=String(v);
    const lab=document.getElementById('v_'+id); if(lab&&el.type==='range') lab.textContent=el.value;
  };
  set('framesize',s.framesize); set('quality',s.quality); set('frameskip',s.frameskip);
  set('hmirror',s.hmirror); set('vflip',s.vflip); set('aec',s.aec); set('agc',s.agc);
  set('smart_ae',s.smart_ae); set('smart_ae_ev',s.smart_ae_ev);
  set('aec_value',s.aec_value); set('agc_gain',s.agc_gain); set('gainceiling',s.gainceiling);
  set('colorbar',s.colorbar); set('mic_gain',s.mic_gain);
  syncSmartAeUi(s);
  if(s.cv_ok){
    const pan=document.getElementById('cv_panel'); if(pan) pan.style.display='block';
    set('cv_mode',s.cv_mode); set('cv_preset',s.cv_preset);
    set('cv_h_lo',s.cv_h_lo); set('cv_h_hi',s.cv_h_hi);
    set('cv_s_lo',s.cv_s_lo); set('cv_s_hi',s.cv_s_hi);
    set('cv_v_lo',s.cv_v_lo); set('cv_v_hi',s.cv_v_hi);
    set('cv_erode',s.cv_erode); set('cv_dilate',s.cv_dilate);
    set('cv_min_area',s.cv_min_area); set('cv_thr',s.cv_thr);
    set('cv_edge_lo',s.cv_edge_lo); set('cv_edge_hi',s.cv_edge_hi);
    set('cv_track_dist',s.cv_track_dist);
    const ch=document.getElementById('cv_hint');
    if(ch) ch.textContent='CV · tracks='+(s.cv_tracks||0)+' · det='+(s.cv_blobs||0)+' · mask_px='+(s.cv_mask_px||0)+' · '+
      (s.cv_ms||0)+'ms · T='+(s.cv_thr||0);
  }
  if(s.face_ok){
    if(!faceBusy){
      set('face_model',s.face_model);
      set('face_detect',s.face_detect!=null?s.face_detect:0);
      set('face_recog',s.face_recog!=null?s.face_recog:0);
    }
    set('face_thr',s.face_thr);
    fillFaceRoster(s.face_roster||'');
    syncFaceToggles();
    setFaceStatus(s);
  }
  syncFaceResLock(s);
  stream.dataset.outRes=String(s.out_w)+'x'+String(s.out_h);
  stream.dataset.faceLock=(s.face_lock || ((s.face_detect||0)|(s.face_recog||0)))?'1':'0';
  applyPreviewNote(s);
  const mg=document.getElementById('mic_gain'); if(mg) mg.disabled=!s.mic_ok;
  const sh=document.getElementById('sensor_hint');
  if(sh){
    const name=s.sensor||'unknown';
    sh.textContent=name.indexOf('OV5647')>=0
      ? ('Sensor: '+name+(s.smart_ae?' · Smart AE on':' · flip syncs ISP Bayer'))
      : ('Sensor: '+name+' — mirror/AEC/AGC knobs are OV5647-only (no-op here)');
  }
  const hint=document.getElementById('sd_hint');
  const btn=document.getElementById('btn_capture_img');
  if(s.sd_ok){
    hint.textContent='Storage ready → '+ (s.sd_folder||'/IMG') +'  (saved '+ (s.saved||0) +')';
    btn.disabled=false;
    if(btn.parentElement) btn.parentElement.style.display='';
    hint.style.display='';
  }else{
    hint.textContent='';
    hint.style.display='none';
    btn.disabled=true;
    if(btn.parentElement) btn.parentElement.style.display='none';
  }
  const vh=document.getElementById('vid_hint');
  const rb=document.getElementById('btn_record');
  if(s.video_ok){
    vh.textContent='MP4 (H.264+AAC) → '+ (s.video_folder||'/VIDEO') +'  (clips '+ (s.videos||0) +')' +
      (s.last_video?(' · '+s.last_video):'') + (s.mic_ok?' · mic on':' · no mic');
    rb.disabled=false;
  }else{
    vh.textContent='Video record disabled (enableVideoRecord)';
    rb.disabled=true;
  }
  setRecUi(!!s.recording, s.rec_ms||0);
  onSaveProgress(s);
}
function metaText(s){
  let t=(s.sensor||'?')+' · out '+s.out_w+'x'+s.out_h+' · q'+s.quality+' · skip'+s.frameskip+
    ' · '+s.jpeg+'B · '+s.encode_ms+'ms · sent '+s.sent;
  if(s.sd_ok) t+=' · SD '+ (s.saved||0);
  if(s.recording) t+=' · REC '+fmtMs(s.rec_ms||0)+' '+ (s.rec_frames||0)+'f';
  else if(s.finalizing) t+=' · saving '+(s.mux_pct||0)+'%';
  else if(s.video_ok) t+=' · VID '+ (s.videos||0);
  if(s.cv_ok) t+=' · CV m'+s.cv_mode+' b'+(s.cv_blobs||0)+' p'+(s.cv_mask_px||0);
  if(s.qr_ok && s.qr_en) t+=' · QR '+(s.qr_n||0);
  return t;
}
async function refreshMetaOnly(){
  if(recBusy || faceBusy) return;
  const tok=++statusToken;
  try{
    const s=await (await fetch('/status?_='+Date.now(),{cache:'no-store'})).json();
    if(tok!==statusToken || faceBusy) return;
    document.getElementById('meta').textContent=metaText(s);
    const fs=document.getElementById('framesize');
    if(fs && document.activeElement!==fs && s.framesize!=null) fs.value=String(s.framesize);
    syncFaceResLock(s);
    const hint=document.getElementById('sd_hint');
    const btn=document.getElementById('btn_capture_img');
    if(s.sd_ok){
      hint.textContent='Storage ready → '+ (s.sd_folder||'/IMG') +'  (saved '+ (s.saved||0) +')' +
        (s.last_saved?(' · '+s.last_saved):'');
      btn.disabled=false;
      if(btn && btn.parentElement) btn.parentElement.style.display='';
      hint.style.display='';
    }else{
      hint.textContent='';
      hint.style.display='none';
      btn.disabled=true;
      if(btn && btn.parentElement) btn.parentElement.style.display='none';
    }
    const vh=document.getElementById('vid_hint');
    const rb=document.getElementById('btn_record');
    if(s.video_ok){
      vh.textContent='MP4 (H.264+AAC) → '+ (s.video_folder||'/VIDEO') +'  (clips '+ (s.videos||0) +')' +
        (s.last_video?(' · '+s.last_video):'') + (s.mic_ok?' · mic on':' · no mic') +
        (s.finalizing?(' · saving '+(s.mux_pct||0)+'%'):'');
      if(!recBusy && !s.finalizing) rb.disabled=false;
    }else{
      vh.textContent='Video record disabled (enableVideoRecord) — need camera UI on port 80';
      rb.disabled=true;
    }
    if(!recBusy) setRecUi(!!s.recording, s.rec_ms||0);
    onSaveProgress(s);
    syncSmartAeUi(s);
    if(s.cv_ok){
      const ch=document.getElementById('cv_hint');
      if(ch) ch.textContent='CV live · blobs='+(s.cv_blobs||0)+' · mask_px='+(s.cv_mask_px||0)+' · '+
        (s.cv_ms||0)+'ms — Mask=green match. Blobs=boxes. Tune S/V lo if empty.';
    }
    if(s.face_ok){
      setFaceStatus(s);
      fillFaceRoster(s.face_roster||'');
      if(!faceBusy){
        const fd=document.getElementById('face_detect'), fr=document.getElementById('face_recog');
        const fm=document.getElementById('face_model');
        if(fd && document.activeElement!==fd) setFaceOn(fd, !!(s.face_detect));
        if(fr && document.activeElement!==fr) setFaceOn(fr, !!(s.face_recog));
        if(fm && document.activeElement!==fm && s.face_model!=null) fm.value=String(s.face_model);
        syncFaceToggles();
      }
    }
    if(s.qr_ok){
      setQrStatus(s);
      const qe=document.getElementById('qr_en');
      if(qe && document.activeElement!==qe) setFaceOn(qe, !!(s.qr_en));
    }
    if(s.det_ok){
      setDetStatus(s);
      const de=document.getElementById('det_en');
      if(de && document.activeElement!==de) setFaceOn(de, !!(s.det_en));
    }
    applyPreviewNote(s);
  }catch(e){}
}
async function boot(){
  const fp=(window.CAM_FILES_PORT|0);
  if(fp>0){
    const u=filesUrl();
    const a1=document.getElementById('a_files'), a2=document.getElementById('a_files2');
    if(a1){ a1.href=u; a1.style.display=''; }
    if(a2){ a2.href=u; a2.style.display=''; }
  }
  function hideFeat(paneId, btnId){
    const p=document.getElementById(paneId); if(p) p.style.display='none';
    const b=document.getElementById(btnId); if(b) b.style.display='none';
  }
  if(!window.CAM_FACE) hideFeat('tab_face','tabbtn_face');
  if(!window.CAM_DET) hideFeat('tab_det','tabbtn_det');
  if(!window.CAM_QR) hideFeat('tab_qr','tabbtn_qr');
  if(!window.CAM_VIDEO) hideFeat('tab_rec','tabbtn_rec');
  if(window.CAM_CV){ const pan=document.getElementById('cv_panel'); if(pan) pan.style.display='block'; }
  if(window.CAM_FACE){ showTab('tab_face'); }
  if(window.CAM_DET){
    if(window.CAM_DET_TAB){ const b=document.getElementById('tabbtn_det'); if(b) b.textContent=window.CAM_DET_TAB; }
    if(window.CAM_DET_TITLE){ const h=document.querySelector('#tab_det h2'); if(h) h.textContent=window.CAM_DET_TITLE; }
    if(window.CAM_DET_HINT){ const h=document.querySelector('#tab_det .hint'); if(h) h.textContent=window.CAM_DET_HINT; }
    if(window.CAM_DET_OPTS && window.CAM_DET_OPTS.length){
      const dm=document.getElementById('det_model');
      if(dm){ dm.innerHTML=window.CAM_DET_OPTS.map(o=>'<option value="'+o.v+'">'+o.l+'</option>').join(''); }
    }
    showTab('tab_det');
  }
  if(window.CAM_QR){ document.body.classList.add('qr-mode'); showTab('tab_qr'); }
  buildQrFmtLists();
  try{
    const s=await (await fetch('/status?_='+Date.now(),{cache:'no-store'})).json();
    fillForm(s);
    document.getElementById('meta').textContent=metaText(s);
  }catch(e){document.getElementById('meta').textContent='status error — is port '+basePort+' free?'}
  reconnectStream();
}
boot();
setInterval(()=>{ if(!applying) refreshMetaOnly(); },500);
</script>
</body>
</html>
)HTML";

static const struct {
  uint16_t w, h;
} kStreamSizes[ESP32P4_STREAM_COUNT] = {
    {1920, 1088},  // FHD (height ↑ to ÷16; letterbox from 1080)
    {1280, 720},   // HD 16:9 (exact 2/3 of 1920x1080)
    {1024, 576},   // XGA 16:9
    {800, 448},    // SVGA-ish 16:9 (was 800x640 — mismatched 1080p → bottom trash)
    {640, 368},    // VGA-ish 16:9 (640x360→368)
    {480, 272},    // HVGA 16:9
    {400, 224},    // CIF-ish 16:9
    {320, 176},    // QVGA 16:9
    {240, 128},    // HQVGA 16:9
    {160, 96},     // QQVGA 16:9
};

static void framesizeToWH(uint8_t fs, uint16_t nw, uint16_t nh, uint16_t *w, uint16_t *h) {
  if (nw < 16) nw = 16;
  if (nh < 16) nh = 16;
  if (fs >= ESP32P4_STREAM_COUNT) fs = ESP32P4_STREAM_HD;

  // Pick requested size, or the largest table entry that fits the sensor.
  uint16_t tw = kStreamSizes[fs].w;
  uint16_t th = kStreamSizes[fs].h;
  if (tw <= nw && th <= nh) {
    *w = tw;
    *h = th;
    return;
  }
  for (int i = (int)fs; i >= 0; --i) {
    if (kStreamSizes[i].w <= nw && kStreamSizes[i].h <= nh) {
      *w = kStreamSizes[i].w;
      *h = kStreamSizes[i].h;
      return;
    }
  }
  for (int i = 0; i < (int)ESP32P4_STREAM_COUNT; ++i) {
    if (kStreamSizes[i].w <= nw && kStreamSizes[i].h <= nh) {
      *w = kStreamSizes[i].w;
      *h = kStreamSizes[i].h;
      return;
    }
  }
  // Last resort: floor sensor to ÷16
  *w = (uint16_t)((nw / 16) * 16);
  *h = (uint16_t)((nh / 16) * 16);
  if (*w < 16) *w = 16;
  if (*h < 16) *h = 16;
}

void ESP32P4_MjpegServer::faceModelToWH(int model, uint16_t *w, uint16_t *h) {
  // Face working frame (stream lock), not raw net tensor size.
  // MSR+MNP: classic QVGA 320×240 (Arduino-ESP32 2.0.x CameraWebServer / esp-face).
  // ESPDet: native square input. MFN still aligns internally to 112×112.
  switch (model) {
    case 1:
      *w = 224;
      *h = 224;
      break;
    case 2:
      *w = 416;
      *h = 416;
      break;
    default:
      *w = 320;
      *h = 240;
      break;
  }
}

void ESP32P4_MjpegServer::applyFaceForcedDims() {
  uint16_t tw = 320, th = 240;
  faceModelToWH(_face.model, &tw, &th);
  if (_out_w != tw || _out_h != th) _size_dirty = true;
  _out_w = tw;
  _out_h = th;
}

void ESP32P4_MjpegServer::applyFramesizeDims() {
  if (faceResLocked()) {
    applyFaceForcedDims();
    return;
  }
  uint16_t nw = 800, nh = 640;
  if (_cam) {
    nw = _cam->width();
    nh = _cam->height();
  }
  uint16_t w = nw, h = nh;
  framesizeToWH(_framesize, nw, nh, &w, &h);
  if (_out_w != w || _out_h != h) _size_dirty = true;
  _out_w = w;
  _out_h = h;
}

bool ESP32P4_MjpegServer::setFramesize(uint8_t fs) {
  if (fs >= ESP32P4_STREAM_COUNT) return false;
  _framesize = fs;
  applyFramesizeDims();
  // Worker clears scale/JPEG buffers on next frame so old larger frames cannot leak.
  _size_dirty = true;
  return true;
}

bool ESP32P4_MjpegServer::begin(ESP32P4_Camera *cam, uint16_t port, uint8_t quality) {
  if (!cam) return false;
  end();
  ESP32P4_Debug::ensure();
  _cam = cam;
  _port = port;
  _stream_port = (uint16_t)(port + 1);
  _quality = quality < 4 ? 4 : (quality > 63 ? 63 : quality);
  if (!_jpeg.begin(cam->width(), cam->height(), _quality)) return false;
  _jpeg.setChroma(ESP32P4_JPEG_CHROMA_YUV420);
  _ppa.begin();
  (void)_smart_ae.begin(_cam);

  _framesize = ESP32P4_STREAM_HD;
  applyFramesizeDims();
  // If HD does not fit (e.g. 800x640 sensor), applyFramesizeDims already clamped.

  // JPEG buffer scales with stream size (1080p native needs more headroom).
  size_t need_jpg = (size_t)_out_w * (size_t)_out_h / 2;
  if (need_jpg < 220 * 1024) need_jpg = 220 * 1024;
  if (need_jpg > 900 * 1024) need_jpg = 900 * 1024;
  _jpg_cap = need_jpg;
  for (int i = 0; i < 2; i++) {
    _jpg_buf[i] = (uint8_t *)esp32p4_psram_alloc(_jpg_cap);
    if (!_jpg_buf[i]) {
      end();
      return false;
    }
    _jpg_len[i] = 0;
  }
  _tx_buf = (uint8_t *)esp32p4_psram_alloc(_jpg_cap);
  if (!_tx_buf) {
    end();
    return false;
  }
  _scale_cap = (size_t)cam->width() * cam->height() * 2;
  _scale_buf = (uint8_t *)esp32p4_psram_alloc(_scale_cap);
  if (!_scale_buf) {
    end();
    return false;
  }

  _ready_idx = -1;
  _enc_idx = 0;
  _frame_seq = 0;
  _frame_sem = xSemaphoreCreateBinary();
  _jpg_mutex = xSemaphoreCreateMutex();
  _rec_mutex = xSemaphoreCreateMutex();
  if (!_frame_sem || !_jpg_mutex || !_rec_mutex) {
    end();
    return false;
  }

  // Control / UI server — never hosts blocking MJPEG.
  _http = new WebServer(_port);
  _http->on("/", HTTP_GET, [this]() { handleRoot(); });
  _http->on("/jpg", HTTP_GET, [this]() { handleJpg(); });
  _http->on("/capture", HTTP_GET, [this]() { handleCapture(); });
  _http->on("/capture_img", HTTP_GET, [this]() { handleCaptureImg(); });
  _http->on("/record/start", HTTP_GET, [this]() { handleRecordStart(); });
  _http->on("/record/stop", HTTP_GET, [this]() { handleRecordStop(); });
  _http->on("/audio", HTTP_GET, [this]() { handleAudio(); });
  _http->on("/status", HTTP_GET, [this]() { handleStatus(); });
  _http->on("/debug", HTTP_GET, [this]() { handleDebug(); });
  _http->on("/control", HTTP_GET, [this]() { handleControl(); });
  _http->begin();

  // Dedicated stream port so handleStream cannot block /control.
  _stream_http = new WebServer(_stream_port);
  _stream_http->on("/stream", HTTP_GET, [this]() { handleStream(); });
  _stream_http->on("/", HTTP_GET, [this]() {
    _stream_http->send(200, "text/plain", "MJPEG on /stream — open UI on control port");
  });
  _stream_http->begin();

  _audio_http = new WebServer(_audio_port);
  _audio_http->on("/audio.pcm", HTTP_GET, [this]() { handleAudioStream(false); });
  _audio_http->on("/audio.wav", HTTP_GET, [this]() { handleAudioStream(true); });
  _audio_http->on("/", HTTP_GET, [this]() {
    _audio_http->send(200, "text/plain", "Live PCM on /audio.pcm (low latency) or /audio.wav");
  });
  _audio_http->begin();

  esp_wifi_set_ps(WIFI_PS_NONE);
  if (!startWorker()) return false;
  return startHttpTasks();
}

void ESP32P4_MjpegServer::loop() {
  ESP32P4_Debug::poll();
  CSI_DBG(ESP32P4_DBG_STREAM, "sent=%u drop=%u jpeg=%u enc=%ums phase=%u age=%u busy=%u ppa_to=%u",
          (unsigned)_sent, (unsigned)_dropped, (unsigned)_last_jpeg, (unsigned)_encode_ms,
          (unsigned)_work_phase, (unsigned)lastFrameAgeMs(), (unsigned)jpgBusyMask(),
          (unsigned)_ppa.timeoutCount());
  delay(1);
}

void ESP32P4_MjpegServer::end() {
  stopHttpTasks();
  stopWorker();
  stopMicTask();
  disableVideoRecord();
  disableMic();
  disableSdCapture();
  if (_cv_on) {
    _cv_on = false;
    ESP32P4_CvDash::release();
  }
  if (_http) {
    _http->stop();
    delete _http;
    _http = nullptr;
  }
  if (_stream_http) {
    _stream_http->stop();
    delete _stream_http;
    _stream_http = nullptr;
  }
  if (_audio_http) {
    _audio_http->stop();
    delete _audio_http;
    _audio_http = nullptr;
  }
  _jpeg.end();
  _ppa.end();
  _smart_ae.end();
  for (int i = 0; i < 2; i++) {
    esp32p4_psram_free(_jpg_buf[i]);
    _jpg_buf[i] = nullptr;
    _jpg_len[i] = 0;
  }
  esp32p4_psram_free(_tx_buf);
  _tx_buf = nullptr;
  esp32p4_psram_free(_scale_buf);
  _scale_buf = nullptr;
  _scale_cap = 0;
  if (_frame_sem) {
    vSemaphoreDelete(_frame_sem);
    _frame_sem = nullptr;
  }
  if (_jpg_mutex) {
    vSemaphoreDelete(_jpg_mutex);
    _jpg_mutex = nullptr;
  }
  if (_rec_mutex) {
    vSemaphoreDelete(_rec_mutex);
    _rec_mutex = nullptr;
  }
}

void ESP32P4_MjpegServer::setQuality(uint8_t q) {
  _quality = q < 4 ? 4 : (q > 63 ? 63 : q);
  _jpeg.setQuality(_quality);
}

void ESP32P4_MjpegServer::setFrameSkip(uint8_t skip) {
  _frame_skip = skip > 8 ? 8 : skip;
}

bool ESP32P4_MjpegServer::enableSdCapture(ESP32P4_Sd *sd, const char *folder) {
  return enableCapture((sd && sd->mounted()) ? &sd->fs() : nullptr, folder);
}

bool ESP32P4_MjpegServer::enableSdCapture(fs::FS *fs, const char *folder) {
  return enableCapture(fs, folder);
}

bool ESP32P4_MjpegServer::enableCapture(fs::FS *fs, const char *folder) {
  if (!fs) {
    Serial.println("MJPEG: enableCapture needs a mounted fs::FS (SD / FFat / LittleFS / ...)");
    return false;
  }
  if (!_jpg_cap) {
    Serial.println("MJPEG: call begin() before enableCapture");
    return false;
  }

  disableSdCapture();
  _store = fs;
  if (!folder || !folder[0]) folder = "/IMG";
  strncpy(_sd_folder, folder, sizeof(_sd_folder) - 1);
  _sd_folder[sizeof(_sd_folder) - 1] = '\0';
  _last_saved[0] = '\0';
  _saved = 0;
  _save_index = 0;

  if (!_store->exists(_sd_folder)) {
    if (!_store->mkdir(_sd_folder)) {
      Serial.printf("MJPEG: mkdir %s failed\n", _sd_folder);
      _store = nullptr;
      return false;
    }
  }

  _save_cap = _jpg_cap;
  _save_buf = (uint8_t *)esp32p4_psram_alloc(_save_cap);
  if (!_save_buf) {
    Serial.println("MJPEG: save buffer alloc failed");
    _store = nullptr;
    return false;
  }

  // Continue numbering if files already exist (IMG_00001.jpg …)
  for (uint32_t i = 1; i < 100000; i++) {
    char path[64];
    snprintf(path, sizeof(path), "%s/IMG_%05lu.jpg", _sd_folder, (unsigned long)i);
    if (!_store->exists(path)) {
      _save_index = i - 1;
      break;
    }
  }

  Serial.printf("MJPEG: capture -> %s  next=IMG_%05lu.jpg\n", _sd_folder,
                (unsigned long)(_save_index + 1));
  return true;
}

void ESP32P4_MjpegServer::disableSdCapture() {
  _store = nullptr;
  esp32p4_psram_free(_save_buf);
  _save_buf = nullptr;
  _save_cap = 0;
  _last_saved[0] = '\0';
}

bool ESP32P4_MjpegServer::enableVideoRecord(ESP32P4_Sd *sd, ESP32P4_H264 *h264, const char *folder) {
  return enableVideoRecord((sd && sd->mounted()) ? &sd->fs() : nullptr, h264, folder);
}

bool ESP32P4_MjpegServer::enableVideoRecord(fs::FS *fs, ESP32P4_H264 *h264, const char *folder) {
  if (!fs || !h264 || !h264->ready()) {
    Serial.println("MJPEG: enableVideoRecord needs mounted FS + ready H264");
    return false;
  }
  if (!_cam) {
    Serial.println("MJPEG: call begin() before enableVideoRecord");
    return false;
  }

  disableVideoRecord();
  _rec_fs = fs;
  _h264 = h264;
  if (!folder || !folder[0]) folder = "/VIDEO";
  strncpy(_video_folder, folder, sizeof(_video_folder) - 1);
  _video_folder[sizeof(_video_folder) - 1] = '\0';
  _last_video[0] = '\0';
  _videos = 0;
  _video_index = 0;
  _recording = false;

  if (!_rec_fs->exists(_video_folder)) {
    if (!_rec_fs->mkdir(_video_folder)) {
      Serial.printf("MJPEG: mkdir %s failed\n", _video_folder);
      _rec_fs = nullptr;
      _h264 = nullptr;
      return false;
    }
  }

  {
    File dir = _rec_fs->open(_video_folder);
    if (dir && dir.isDirectory()) {
      File f = dir.openNextFile();
      while (f) {
        const char *raw = f.name();
        const char *base = raw ? strrchr(raw, '/') : nullptr;
        base = base ? (base + 1) : (raw ? raw : "");
        const size_t sz = f.size();
        const bool stale = (base[0] && mjpeg_is_rec_work(base)) || mjpeg_ends_ci(base, ".mp4.tmp") ||
                           (mjpeg_ends_ci(base, ".mp4") && sz == 0);
        f.close();
        if (stale) {
          char path[96];
          snprintf(path, sizeof(path), "%s/%s", _video_folder, base);
          _rec_fs->remove(path);
          Serial.printf("MJPEG: removed leftover %s\n", path);
        }
        f = dir.openNextFile();
      }
    }
    if (dir) dir.close();
  }

  _rec_scale_cap = (size_t)h264->width() * h264->height() * 2;
  _rec_scale_buf = (uint8_t *)esp32p4_psram_alloc(_rec_scale_cap);
  if (!_rec_scale_buf) {
    Serial.println("MJPEG: rec scale buffer alloc failed");
    _rec_fs = nullptr;
    _h264 = nullptr;
    return false;
  }

  for (uint32_t i = 1; i < 100000; i++) {
    char path[64];
    snprintf(path, sizeof(path), "%s/VID_%05lu.mp4", _video_folder, (unsigned long)i);
    if (!_rec_fs->exists(path)) {
      _video_index = i - 1;
      break;
    }
  }

  Serial.printf("MJPEG: video record -> %s  next=VID_%05lu.mp4  enc=%ux%u\n", _video_folder,
                (unsigned long)(_video_index + 1), (unsigned)h264->width(),
                (unsigned)h264->height());
  return true;
}

void ESP32P4_MjpegServer::disableVideoRecord() {
  if (_recording) stopVideoRecord();
  _recording = false;
  uint32_t t0 = millis();
  while (_rec_finalizing && (millis() - t0) < 60000) vTaskDelay(pdMS_TO_TICKS(20));
  _h264 = nullptr;
  _rec_fs = nullptr;
  esp32p4_psram_free(_rec_scale_buf);
  _rec_scale_buf = nullptr;
  _rec_scale_cap = 0;
  _last_video[0] = '\0';
}

bool ESP32P4_MjpegServer::enableMic(ESP32P4_Mic *mic) {
  if (!mic || !mic->ready()) {
    Serial.println("MJPEG: enableMic needs ready ESP32P4_Mic");
    return false;
  }
  _mic = mic;
  if (!startMicTask()) {
    Serial.println("MJPEG: mic task FAILED - waveform may stall HTTP");
  }
  Serial.printf("MJPEG: mic enabled @ %d Hz (waveform + MP4 AAC)\n", mic->sampleRate());
  return true;
}

void ESP32P4_MjpegServer::disableMic() {
  stopMicTask();
  if (_mic && _mic->pcmFileOpen()) _mic->stopPcmFile();
  if (_mic) _mic->freePcmRam();
  _mic = nullptr;
}

bool ESP32P4_MjpegServer::startMicTask() {
  if (_mic_task) return true;
  if (!_mic || !_mic->ready()) return false;
  _mic_task_run = true;
  if (xTaskCreatePinnedToCore(micThunk, "p4cam_mic", 4096, this, 3, &_mic_task, 0) != pdPASS) {
    _mic_task_run = false;
    _mic_task = nullptr;
    return false;
  }
  return true;
}

void ESP32P4_MjpegServer::stopMicTask() {
  _mic_task_run = false;
  for (int i = 0; i < 50 && _mic_task; i++) vTaskDelay(pdMS_TO_TICKS(10));
  if (_mic_task) {
    vTaskDelete(_mic_task);
    _mic_task = nullptr;
  }
}

void ESP32P4_MjpegServer::micThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->micLoop();
}

void ESP32P4_MjpegServer::micLoop() {
  while (_mic_task_run) {
    if (_mic && _mic->ready()) {
      // Drain a few short non-blocking-ish reads; never run on the HTTP task.
      for (int i = 0; i < 8; i++) _mic->poll();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  _mic_task = nullptr;
  vTaskDelete(nullptr);
}

bool ESP32P4_MjpegServer::nextVideoPath(char *out, size_t out_cap) {
  if (!_rec_fs || !out || !out_cap) return false;
  for (uint32_t i = _video_index + 1; i < 100000; i++) {
    snprintf(out, out_cap, "%s/VID_%05lu.mp4", _video_folder, (unsigned long)i);
    if (!_rec_fs->exists(out)) {
      _video_index = i;
      return true;
    }
  }
  return false;
}

bool ESP32P4_MjpegServer::startVideoRecord() {
  if (!videoRecordEnabled()) {
    Serial.println("MJPEG: REC start rejected (video record not enabled)");
    return false;
  }
  if (_rec_finalizing) {
    Serial.println("MJPEG: REC start rejected (still finalizing previous clip)");
    return false;
  }
  if (_recording) return true;
  const bool was_paused = _enc_paused;
  _enc_paused = true;
  vTaskDelay(pdMS_TO_TICKS(40));
  if (_rec_mutex && xSemaphoreTake(_rec_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    _enc_paused = was_paused;
    Serial.println("MJPEG: REC start rejected (encoder busy)");
    return false;
  }
  bool ok = false;
  char path[64];
  if (!nextVideoPath(path, sizeof(path))) {
    Serial.println("MJPEG: REC start failed (no free /VIDEO path)");
  } else {
    if (_mic && _mic->ready()) {
      if (!_mic->startPcmRam()) {
        Serial.println("MJPEG: PCM PSRAM failed - recording video-only");
      }
    }
    if (_h264->openMp4(_rec_fs, path)) {
      _rec_save_ok = 0;
      strncpy(_last_video, path, sizeof(_last_video) - 1);
      _last_video[sizeof(_last_video) - 1] = '\0';
      _recording = true;
      ok = true;
      Serial.printf("MJPEG: REC start %s%s\n", path, (_mic && _mic->pcmRam()) ? " +mic" : "");
    } else {
      Serial.printf("MJPEG: openMp4 failed for %s\n", path);
      if (_mic) {
        _mic->stopPcmFile();
        _mic->freePcmRam();
      }
    }
  }
  if (_rec_mutex) xSemaphoreGive(_rec_mutex);
  _enc_paused = false;
  return ok;
}

bool ESP32P4_MjpegServer::stopVideoRecord() {
  // Sync stop (disable / teardown). HTTP Stop uses async finalize instead.
  if (!_recording && !_rec_finalizing) return false;
  _recording = false;
  if (_rec_mutex) xSemaphoreTake(_rec_mutex, pdMS_TO_TICKS(5000));
  if (_mic) {
    _mic->stopPcmFile();
    if (_h264) _h264->setPcmRam(_mic->pcmRam(), _mic->pcmRamBytes(), (uint32_t)_mic->sampleRate());
  }
  bool ok = false;
  if (_h264 && _h264->fileOpen()) {
    _h264->closeFile();
    if (_h264->filePath()[0]) {
      strncpy(_last_video, _h264->filePath(), sizeof(_last_video) - 1);
      _last_video[sizeof(_last_video) - 1] = '\0';
      _videos++;
      ok = true;
      Serial.printf("MJPEG: REC stop %s\n", _last_video);
    }
  }
  if (_mic) _mic->freePcmRam();
  if (_rec_mutex) xSemaphoreGive(_rec_mutex);
  uint32_t t0 = millis();
  while (_rec_finalizing && (millis() - t0) < 60000) vTaskDelay(pdMS_TO_TICKS(20));
  return ok || !_rec_finalizing;
}

void ESP32P4_MjpegServer::finalizeRecThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->finalizeVideoRecord();
  vTaskDelete(nullptr);
}

void ESP32P4_MjpegServer::finalizeVideoRecord() {
  const bool was_paused = _enc_paused;
  _enc_paused = true;
  if (_rec_mutex) xSemaphoreTake(_rec_mutex, portMAX_DELAY);
  if (_mic) {
    _mic->stopPcmFile();
    if (_h264) _h264->setPcmRam(_mic->pcmRam(), _mic->pcmRamBytes(), (uint32_t)_mic->sampleRate());
  }
  if (_h264 && _h264->fileOpen()) {
    _h264->closeFile();
    if (_h264->filePath()[0]) {
      strncpy(_last_video, _h264->filePath(), sizeof(_last_video) - 1);
      _last_video[sizeof(_last_video) - 1] = '\0';
      _videos++;
      Serial.printf("MJPEG: REC stop %s\n", _last_video);
      _rec_save_ok = 1;
    } else {
      _rec_save_ok = -1;
    }
  } else {
    _rec_save_ok = -1;
  }
  if (_mic) _mic->freePcmRam();
  if (_rec_mutex) xSemaphoreGive(_rec_mutex);
  _rec_finalize_task = nullptr;
  _rec_finalizing = false;
  _enc_paused = was_paused;
}

bool ESP32P4_MjpegServer::saveReadyJpegToSd(char *path_out, size_t path_cap, size_t *bytes_out) {
  if (!_store || !_save_buf) return false;

  size_t n = 0;
  if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, portMAX_DELAY);
  const int idx = _ready_idx;
  if (idx >= 0) n = _jpg_len[idx];
  if (idx >= 0 && n > 0 && n <= _save_cap) {
    memcpy(_save_buf, _jpg_buf[idx], n);
  } else {
    n = 0;
  }
  if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);

  if (!n) return false;

  _save_index++;
  char path[64];
  snprintf(path, sizeof(path), "%s/IMG_%05lu.jpg", _sd_folder, (unsigned long)_save_index);
  File f = _store->open(path, FILE_WRITE);
  if (!f || f.write(_save_buf, n) != n) {
    if (f) f.close();
    _save_index--;
    return false;
  }
  f.flush();
  f.close();

  strncpy(_last_saved, path, sizeof(_last_saved) - 1);
  _last_saved[sizeof(_last_saved) - 1] = '\0';
  _saved++;
  if (path_out && path_cap) {
    strncpy(path_out, path, path_cap - 1);
    path_out[path_cap - 1] = '\0';
  }
  if (bytes_out) *bytes_out = n;
  Serial.printf("MJPEG: saved %s (%u bytes)\n", path, (unsigned)n);
  return true;
}

bool ESP32P4_MjpegServer::startHttpTasks() {
  _http_run = true;
  // Higher priority than stream so /control stays snappy.
  if (xTaskCreatePinnedToCore(controlHttpThunk, "p4cam_http", 12288, this, 6, &_control_task, 0) !=
      pdPASS) {
    return false;
  }
  if (xTaskCreatePinnedToCore(streamHttpThunk, "p4cam_strm", 16384, this, 4, &_stream_task, 0) !=
      pdPASS) {
    return false;
  }
  if (xTaskCreatePinnedToCore(audioHttpThunk, "p4cam_aud", 12288, this, 3, &_audio_task, 0) != pdPASS) {
    return false;
  }
  return true;
}

void ESP32P4_MjpegServer::stopHttpTasks() {
  _http_run = false;
  for (int i = 0; i < 50; i++) {
    if (!_control_task && !_stream_task && !_audio_task) break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if (_control_task) {
    vTaskDelete(_control_task);
    _control_task = nullptr;
  }
  if (_stream_task) {
    vTaskDelete(_stream_task);
    _stream_task = nullptr;
  }
  if (_audio_task) {
    vTaskDelete(_audio_task);
    _audio_task = nullptr;
  }
}

void ESP32P4_MjpegServer::controlHttpThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->controlHttpLoop();
}

void ESP32P4_MjpegServer::streamHttpThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->streamHttpLoop();
}

void ESP32P4_MjpegServer::audioHttpThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->audioHttpLoop();
}

void ESP32P4_MjpegServer::controlHttpLoop() {
  while (_http_run) {
    // Mic drain runs on p4cam_mic — keep this task HTTP-only so /record stays responsive.
    if (_http) _http->handleClient();
    vTaskDelay(1);
  }
  _control_task = nullptr;
  vTaskDelete(nullptr);
}

void ESP32P4_MjpegServer::streamHttpLoop() {
  while (_http_run) {
    if (_stream_http) _stream_http->handleClient();
    vTaskDelay(1);
  }
  _stream_task = nullptr;
  vTaskDelete(nullptr);
}

void ESP32P4_MjpegServer::audioHttpLoop() {
  while (_http_run) {
    if (_audio_http) _audio_http->handleClient();
    vTaskDelay(1);
  }
  _audio_task = nullptr;
  vTaskDelete(nullptr);
}

bool ESP32P4_MjpegServer::startWorker() {
  if (_worker) return true;
  _worker_run = true;
  BaseType_t ok = xTaskCreatePinnedToCore(workerThunk, "p4cam_jpg", 32768, this, 5, &_worker, 1);
  return ok == pdPASS;
}

void ESP32P4_MjpegServer::stopWorker() {
  if (!_worker) {
    _worker_run = false;
    return;
  }
  _worker_run = false;
  for (int i = 0; i < 100 && _worker; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (_worker) {
    vTaskDelete(_worker);
    _worker = nullptr;
  }
}

void ESP32P4_MjpegServer::workerThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->workerLoop();
}

void ESP32P4_MjpegServer::workerLoop() {
  uint8_t skip_left = 0;
  uint32_t last_csi_restart = 0;
  _last_fb_ms = millis();
  while (_worker_run) {
    if (_enc_paused) {
      _work_phase = 0;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (_ppa.srmBusy()) {
      _work_phase = 2;
      vTaskDelay(1);
      continue;
    }
    _work_phase = 1;
    camera_fb_t *fb = _cam->capture(80);
    if (!fb) {
      _cap_fail++;
      if (!_ppa.srmBusy() && (millis() - _last_fb_ms) > 2000 &&
          (millis() - last_csi_restart) > 3000) {
        last_csi_restart = millis();
        CSI_STALL(ESP32P4_DBG_CAM, "capture stall age=%ums done=%u drop=%u - restart",
                      (unsigned)lastFrameAgeMs(), (unsigned)_cam->doneCount(),
                      (unsigned)_cam->dropCount());
        _cam->stopCapture();
        vTaskDelay(pdMS_TO_TICKS(20));
        _cam->startCapture();
      }
      vTaskDelay(1);
      continue;
    }
    _last_fb_ms = millis();
    if (skip_left && !_recording) {
      skip_left--;
      _cam->release(fb);
      _dropped++;
      continue;
    }
    if (!_recording) skip_left = _frame_skip;
    else skip_left = 0;

    // Phone-style H.264: encode as fast as possible while REC is on (no fps wait).
    if (_recording && _h264 && _rec_scale_buf) {
      if (fb->format == ESP32P4_PIXFORMAT_RGB565 && _rec_mutex &&
          xSemaphoreTake(_rec_mutex, 0) == pdTRUE) {
        if (_recording && _h264->fileOpen()) {
          const uint16_t rw = _h264->width();
          const uint16_t rh = _h264->height();
          const uint8_t *rrgb = fb->buf;
          uint16_t rew = fb->width, reh = fb->height;
          if (fb->width != rw || fb->height != rh) {
            if (_ppa.scale(fb, _rec_scale_buf, _rec_scale_cap, rw, rh)) {
              rrgb = _rec_scale_buf;
              rew = rw;
              reh = rh;
              _h264->encodeToFile(rrgb, rew, reh);
            }
          } else {
            _h264->encodeToFile(rrgb, rew, reh);
          }
        }
        xSemaphoreGive(_rec_mutex);
      }
    }

    const uint16_t ow = _out_w;
    const uint16_t oh = _out_h;

    if (fb->format == ESP32P4_PIXFORMAT_JPEG) {
      const int i = _enc_idx;
      uint32_t wait0 = millis();
      _work_phase = 4;
      while (_jpg_busy[i] && _worker_run && (millis() - wait0) < 250) {
        vTaskDelay(1);
      }
      const size_t n = fb->len;
      if (_jpg_busy[i] || n == 0 || n > _jpg_cap) {
        _cam->release(fb);
        _dropped++;
        continue;
      }
      memcpy(_jpg_buf[i], fb->buf, n);
      _cam->release(fb);
      if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, portMAX_DELAY);
      _jpg_len[i] = n;
      _last_jpeg = n;
      _ready_idx = i;
      _enc_idx ^= 1;
      _frame_seq++;
      if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);
      xSemaphoreGive(_frame_sem);
      _work_phase = 0;
      continue;
    }

    const uint8_t *rgb = fb->buf;
    uint16_t ew = fb->width;
    uint16_t eh = fb->height;

    if (_size_dirty) {
      _jpeg.clearInput();
      if (_scale_buf && _scale_cap) memset(_scale_buf, 0, _scale_cap);
      if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, portMAX_DELAY);
      _jpg_len[0] = 0;
      _jpg_len[1] = 0;
      _ready_idx = -1;
      if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);
      _size_dirty = false;
    }

    if (ow != fb->width || oh != fb->height) {
      _work_phase = 2;
      bool scaled = false;
      // Wipe dest every scale (face example): leftover overlays cannot sit in
      // letterbox pads after a framesize change.
      const size_t wipe = (size_t)ow * (size_t)oh * 2;
      if (_scale_buf && wipe > 0 && wipe <= _scale_cap) memset(_scale_buf, 0, wipe);
      // QR needs the full sensor FOV (letterbox). Cover-crop chops finder patterns at the edges.
      if (_qr.on) {
        scaled = _ppa.scaleFit(fb, _scale_buf, _scale_cap, ow, oh);
        if (!scaled) scaled = _ppa.scaleCover(fb, _scale_buf, _scale_cap, ow, oh);
      } else {
        // Face/CV: cover-crop so overlays stay inside the visible frame.
        scaled = _ppa.scaleCover(fb, _scale_buf, _scale_cap, ow, oh);
        if (!scaled) scaled = _ppa.scaleFit(fb, _scale_buf, _scale_cap, ow, oh);
      }
      if (!scaled && ow == (uint16_t)(fb->width / 2) && oh == (uint16_t)(fb->height / 2) &&
                 (size_t)ow * oh * 2 <= _scale_cap) {
        ESP32P4_Img::downsample2x565((const uint16_t *)fb->buf, fb->width, fb->height,
                                     (uint16_t *)_scale_buf);
        scaled = true;
      }
      if (!scaled) {
        if (_ppa.srmBusy()) {
          uint32_t stuck0 = millis();
          while (_ppa.srmBusy() && _worker_run && (millis() - stuck0) < 5000) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        _cam->release(fb);
        _dropped++;
        continue;
      }
      rgb = _scale_buf;
      ew = ow;
      eh = oh;
      _cam->release(fb);
      fb = nullptr;
    }

    // Annotate in place (CSI fb or scale_buf). Never full-frame memcpy just for the hook —
    // that copy + QR snap was starving JPEG/TCP and produced half-black MJPEG parts.
    if (_frame_hook && rgb) {
      _frame_hook((uint16_t *)rgb, (int)ew, (int)eh, _frame_hook_user);
    }

    // Software Smart AE on stream buffer (subsampled; rate-limited inside).
    if (_smart_ae.enabled() && rgb) {
      _smart_ae.process((const uint16_t *)rgb, (int)ew, (int)eh);
    }

    const int i = _enc_idx;
    // Do not overwrite a JPEG slot that /stream is still sending (causes freeze/tearing).
    {
      uint32_t wait0 = millis();
      _work_phase = 4;
      while (_jpg_busy[i] && _worker_run && (millis() - wait0) < 250) {
        vTaskDelay(1);
      }
      if (_jpg_busy[i]) {
        if (fb) _cam->release(fb);
        _dropped++;
        continue;
      }
    }
    _work_phase = 3;
    const uint32_t t0 = millis();
    size_t n = _jpeg.encode(rgb, ew, eh, _jpg_buf[i], _jpg_cap);
    _encode_ms = millis() - t0;
    if (fb) {
      _cam->release(fb);
      fb = nullptr;
    }
    if (!n) {
      _jpeg_fail++;
      _dropped++;
      CSI_EVT(ESP32P4_DBG_JPEG, "encode fail n=%u", (unsigned)_jpeg_fail);
      continue;
    }

    if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, portMAX_DELAY);
    _jpg_len[i] = n;
    _last_jpeg = n;
    _ready_idx = i;
    _enc_idx ^= 1;
    _frame_seq++;
    if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);
    xSemaphoreGive(_frame_sem);
    _work_phase = 0;
  }
  _worker = nullptr;
  vTaskDelete(nullptr);
}

void ESP32P4_MjpegServer::setFilesBrowserPort(uint16_t port) { _files_port = port; }

void ESP32P4_MjpegServer::setFrameHook(FrameHook hook, void *user) {
  _frame_hook = hook;
  _frame_hook_user = user;
  if (hook != cvDashHook) _cv_on = false;
}

void ESP32P4_MjpegServer::cvDashHook(uint16_t *rgb, int w, int h, void *user) {
  auto *self = static_cast<ESP32P4_MjpegServer *>(user);
  if (!self || !self->_cv_on) return;
  ESP32P4_CvDash::process(rgb, w, h, self->_cv);
}

bool ESP32P4_MjpegServer::enableCvDashboard(bool on) {
  _cv_on = on;
  if (on) {
    _face.on = false;
    _qr.on = false;
    _det.on = false;
    _cv.mode = ESP32P4_CV_EDGE_TRACK;
    _cv.preset = ESP32P4_CV_PRESET_COINS;
    ESP32P4_CvDash::applyPreset(_cv, _cv.preset);
    setFrameHook(cvDashHook, this);
  } else if (_frame_hook == cvDashHook) {
    clearFrameHook();
    ESP32P4_CvDash::release();
  }
  return true;
}

bool ESP32P4_MjpegServer::enableFaceUi(bool on) {
  _face.on = on;
  if (on) {
    _cv_on = false;
    _qr.on = false;
    _det.on = false;
  }
  return true;
}

bool ESP32P4_MjpegServer::enableQrUi(bool on) {
  _qr.on = on;
  if (on) {
    _cv_on = false;
    _face.on = false;
    _det.on = false;
    if (!_qr.formats) _qr.formats = ESP32P4_Qr::defaultFormats();
  }
  return true;
}

bool ESP32P4_MjpegServer::enableDetUi(bool on) {
  _det.on = on;
  if (on) {
    _cv_on = false;
    _face.on = false;
    _qr.on = false;
  }
  return true;
}

void ESP32P4_MjpegServer::setDetCatalog(const char *tab, const char *title, const char *hint, int n,
                                        const int *values, const char *const *labels) {
  if (tab && tab[0]) {
    strncpy(_det.tab, tab, sizeof(_det.tab) - 1);
    _det.tab[sizeof(_det.tab) - 1] = '\0';
  }
  if (title && title[0]) {
    strncpy(_det.title, title, sizeof(_det.title) - 1);
    _det.title[sizeof(_det.title) - 1] = '\0';
  }
  if (hint && hint[0]) {
    strncpy(_det.hint, hint, sizeof(_det.hint) - 1);
    _det.hint[sizeof(_det.hint) - 1] = '\0';
  }
  if (n < 0) n = 0;
  if (n > 10) n = 10;
  _det.opt_n = n;
  for (int i = 0; i < n; i++) {
    _det.opt_value[i] = values ? values[i] : i;
    _det.opt_label[i] = (labels && labels[i]) ? labels[i] : "?";
  }
  if (n > 0) _det.model = _det.opt_value[0];
}

void ESP32P4_MjpegServer::setPreviewNote(const char *msg) {
  if (!msg) msg = "";
  strncpy(_preview_note, msg, sizeof(_preview_note) - 1);
  _preview_note[sizeof(_preview_note) - 1] = '\0';
}

void ESP32P4_MjpegServer::setModelMissingNote(const char *volumes, const char *what) {
  char buf[160];
  snprintf(buf, sizeof(buf), "%s missing. Upload /models/p4/*.espdl to %s via Files.",
           (what && what[0]) ? what : "Model",
           (volumes && volumes[0]) ? volumes : "SD/FFat/SPIFFS");
  setPreviewNote(buf);
}

bool ESP32P4_MjpegServer::enableSmartAe(bool on) {
  if (!_cam) return false;
  if (!_smart_ae.begin(_cam)) return false;
  _smart_ae.setEnabled(on);
  if (!on) (void)_cam->setAeEvBias(_smart_ae.evBias());
  return true;
}

static size_t jsonQuote(char *dst, size_t cap, size_t o, const char *s) {
  if (o + 2 >= cap) return o;
  dst[o++] = '"';
  if (s) {
    for (; *s && o + 2 < cap; ++s) {
      if (*s == '"' || *s == '\\') {
        if (o + 3 >= cap) break;
        dst[o++] = '\\';
      }
      if ((unsigned char)*s < 32) continue;
      dst[o++] = *s;
    }
  }
  if (o < cap) dst[o++] = '"';
  return o;
}

void ESP32P4_MjpegServer::handleRoot() {
  _http->setContentLength(CONTENT_LENGTH_UNKNOWN);
  _http->send(200, "text/html; charset=utf-8", "");
  char boot[256];
  snprintf(boot, sizeof(boot),
           "<script>window.CAM_FILES_PORT=%u;window.CAM_CV=%s;window.CAM_FACE=%s;window.CAM_QR=%s;window.CAM_DET=%s;window.CAM_VIDEO=%s;</script>",
           (unsigned)_files_port, _cv_on ? "true" : "false", _face.on ? "true" : "false",
           _qr.on ? "true" : "false", _det.on ? "true" : "false",
           videoRecordEnabled() ? "true" : "false");
  _http->sendContent(boot);
  if (_det.on) {
    char extra[1024];
    size_t o = 0;
    memcpy(extra, "<script>window.CAM_DET_TAB=", 27);
    o = 27;
    o = jsonQuote(extra, sizeof(extra), o, _det.tab);
    memcpy(extra + o, ";window.CAM_DET_TITLE=", 22);
    o += 22;
    o = jsonQuote(extra, sizeof(extra), o, _det.title);
    memcpy(extra + o, ";window.CAM_DET_HINT=", 21);
    o += 21;
    o = jsonQuote(extra, sizeof(extra), o, _det.hint);
    memcpy(extra + o, ";window.CAM_DET_OPTS=[", 22);
    o += 22;
    for (int i = 0; i < _det.opt_n && o + 48 < sizeof(extra); i++) {
      if (i) extra[o++] = ',';
      o += (size_t)snprintf(extra + o, sizeof(extra) - o, "{\"v\":%d,\"l\":", _det.opt_value[i]);
      o = jsonQuote(extra, sizeof(extra), o, _det.opt_label[i] ? _det.opt_label[i] : "");
      if (o < sizeof(extra)) extra[o++] = '}';
    }
    if (o + 12 > sizeof(extra)) o = sizeof(extra) - 12;
    memcpy(extra + o, "];</script>", 11);
    o += 11;
    extra[o] = '\0';
    _http->sendContent(extra);
  }
  _http->sendContent_P(INDEX_HTML);
}

void ESP32P4_MjpegServer::sendJpeg(WebServer *srv) {
  if (!srv) return;
  int idx = -1;
  size_t n = 0;
  if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, pdMS_TO_TICKS(50));
  idx = _ready_idx;
  if (idx >= 0 && idx <= 1) {
    n = _jpg_len[idx];
    if (n) _jpg_busy[idx] = 1;
  }
  if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);

  if (idx < 0 || !n) {
    srv->send(503, "text/plain", "no frame yet");
    return;
  }
  WiFiClient client = srv->client();
  client.printf(
      "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n"
      "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n"
      "Connection: close\r\n\r\n",
      (unsigned)n);
  client.write(_jpg_buf[idx], n);
  _jpg_busy[idx] = 0;
}

void ESP32P4_MjpegServer::handleJpg() { sendJpeg(_http); }

void ESP32P4_MjpegServer::handleCapture() {
  int idx = _ready_idx;
  size_t n = 0;
  if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, pdMS_TO_TICKS(50));
  idx = _ready_idx;
  if (idx >= 0) n = _jpg_len[idx];
  if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);
  if (idx < 0 || !n) {
    _http->send(503, "text/plain", "no frame yet");
    return;
  }
  WiFiClient client = _http->client();
  client.printf(
      "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n"
      "Content-Disposition: inline; filename=capture.jpg\r\n"
      "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
      (unsigned)n);
  client.write(_jpg_buf[idx], n);
}

void ESP32P4_MjpegServer::handleCaptureImg() {
  if (!sdCaptureEnabled()) {
    _http->send(503, "application/json", "{\"ok\":0,\"error\":\"SD not ready\"}");
    return;
  }
  char path[64];
  size_t bytes = 0;
  if (!saveReadyJpegToSd(path, sizeof(path), &bytes)) {
    _http->send(503, "application/json", "{\"ok\":0,\"error\":\"no frame or write failed\"}");
    return;
  }
  char buf[192];
  snprintf(buf, sizeof(buf), "{\"ok\":1,\"path\":\"%s\",\"bytes\":%u,\"saved\":%u}\n", path,
           (unsigned)bytes, (unsigned)_saved);
  _http->send(200, "application/json", buf);
}

void ESP32P4_MjpegServer::handleRecordStart() {
  if (!videoRecordEnabled()) {
    _http->send(503, "application/json", "{\"ok\":0,\"error\":\"video record not enabled\"}");
    return;
  }
  if (_recording) {
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"ok\":1,\"recording\":1,\"path\":\"%s\",\"elapsed_ms\":%u,\"frames\":%u}\n",
             _last_video, (unsigned)(_h264 ? _h264->recordElapsedMs() : 0),
             (unsigned)(_h264 ? _h264->framesEncoded() : 0));
    _http->send(200, "application/json", buf);
    return;
  }
  if (!startVideoRecord()) {
    _http->send(503, "application/json", "{\"ok\":0,\"error\":\"start failed\"}");
    return;
  }
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"ok\":1,\"recording\":1,\"path\":\"%s\",\"elapsed_ms\":0,\"frames\":0}\n", _last_video);
  _http->send(200, "application/json", buf);
}

void ESP32P4_MjpegServer::handleRecordStop() {
  if (!videoRecordEnabled() && !_recording && !_rec_finalizing) {
    _http->send(503, "application/json", "{\"ok\":0,\"error\":\"video record not enabled\"}");
    return;
  }
  if (_rec_finalizing) {
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"ok\":1,\"recording\":0,\"finalizing\":1,\"path\":\"%s\",\"videos\":%u}\n",
             _last_video, (unsigned)_videos);
    _http->send(200, "application/json", buf);
    return;
  }
  if (!_recording) {
    char buf[192];
    snprintf(buf, sizeof(buf), "{\"ok\":1,\"recording\":0,\"path\":\"%s\",\"videos\":%u}\n",
             _last_video, (unsigned)_videos);
    _http->send(200, "application/json", buf);
    return;
  }

  // Stop encode immediately; mux on a side task so /record/stop returns fast.
  _recording = false;
  if (_rec_mutex && xSemaphoreTake(_rec_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    if (_mic) _mic->stopPcmFile();
    xSemaphoreGive(_rec_mutex);
  } else if (_mic) {
    _mic->stopPcmFile();
  }

  char pending[64];
  strncpy(pending, _last_video, sizeof(pending) - 1);
  pending[sizeof(pending) - 1] = '\0';

  _rec_finalizing = true;
  BaseType_t created =
      xTaskCreatePinnedToCore(finalizeRecThunk, "rec_mux", 12288, this, 2, &_rec_finalize_task, 0);
  if (created != pdPASS) {
    _rec_finalize_task = nullptr;
    // Fallback: sync mux on this task so the clip is not lost.
    finalizeVideoRecord();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":1,\"recording\":0,\"path\":\"%s\",\"bytes\":%llu,\"videos\":%u}\n",
             _last_video, (unsigned long long)(_h264 ? _h264->fileBytes() : 0), (unsigned)_videos);
    _http->send(200, "application/json", buf);
    return;
  }

  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"ok\":1,\"recording\":0,\"finalizing\":1,\"path\":\"%s\",\"videos\":%u}\n", pending,
           (unsigned)_videos);
  _http->send(200, "application/json", buf);
}

// Arduino WiFiClient::write() can block ~10s per call (10 × 1s select).
// That wedged /stream even when the 800ms frame budget thought it had timed out.
static size_t mjpeg_sock_send(WiFiClient &c, const uint8_t *p, size_t n, uint32_t budget_ms,
                              uint32_t stall_ms = 0) {
  int sock = c.fd();
  if (sock < 0 || !p || !n) return 0;
  size_t off = 0;
  const uint32_t t0 = millis();
  uint32_t last_prog = t0;
  while (off < n && c.connected()) {
    const uint32_t now = millis();
    if (budget_ms && (now - t0) > budget_ms) break;
    if (stall_ms && (now - last_prog) > stall_ms) break;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sock, &set);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 20000;
    int sel = ::select(sock + 1, nullptr, &set, nullptr, &tv);
    if (sel <= 0) continue;
    int w = ::send(sock, p + off, n - off, MSG_DONTWAIT);
    if (w > 0) {
      off += (size_t)w;
      last_prog = millis();
      continue;
    }
    if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK) break;
  }
  return off;
}

void ESP32P4_MjpegServer::handleStream() {
  WiFiClient client = _stream_http->client();
  client.setNoDelay(true);
  client.setTimeout(50);
  CSI_EVT(ESP32P4_DBG_STREAM, "client open");
  static const char kHdr[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
      "Cache-Control: no-cache, no-store, must-revalidate\r\n"
      "Pragma: no-cache\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Connection: close\r\n\r\n";
  if (mjpeg_sock_send(client, (const uint8_t *)kHdr, sizeof(kHdr) - 1, 400) != sizeof(kHdr) - 1) {
    CSI_STALL(ESP32P4_DBG_NET, "stream header send fail");
    client.stop();
    return;
  }

  uint32_t last_seq = 0;
  while (client.connected() && _http_run) {
    if (_frame_seq == last_seq) {
      if (xSemaphoreTake(_frame_sem, pdMS_TO_TICKS(200)) != pdTRUE) {
        vTaskDelay(1);
        continue;
      }
    }
    while (xSemaphoreTake(_frame_sem, 0) == pdTRUE) {
    }
    last_seq = _frame_seq;
    int idx = -1;
    size_t n = 0;
    if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, pdMS_TO_TICKS(50));
    idx = _ready_idx;
    if (idx >= 0 && idx <= 1) n = _jpg_len[idx];
    if (n && n <= _jpg_cap && _tx_buf) memcpy(_tx_buf, _jpg_buf[idx], n);
    if (_jpg_mutex) xSemaphoreGive(_jpg_mutex);
    if (idx < 0 || !n || !_tx_buf) continue;

    // Do not send Content-Length unless TCP can take data. A partial JPEG body
    // freezes the browser until reconnect.
    {
      int sock = client.fd();
      if (sock < 0) break;
      fd_set set;
      FD_ZERO(&set);
      FD_SET(sock, &set);
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 25000;
      if (::select(sock + 1, nullptr, &set, nullptr, &tv) <= 0) {
        CSI_DBG(ESP32P4_DBG_NET, "skip jpeg - tcp congested n=%u", (unsigned)n);
        continue;
      }
    }

    char mh[96];
    int mhlen = snprintf(mh, sizeof(mh),
                         "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                         (unsigned)n);
    if (mhlen < 0 ||
        mjpeg_sock_send(client, (const uint8_t *)mh, (size_t)mhlen, 200) != (size_t)mhlen) {
      CSI_STALL(ESP32P4_DBG_NET, "part header fail n=%u", (unsigned)n);
      break;
    }
    const uint32_t t0 = millis();
    // No total budget: abort only if TCP makes zero progress for 250 ms.
    size_t sentn = mjpeg_sock_send(client, _tx_buf, n, 0, 250);
    const uint32_t dt = millis() - t0;
    if (sentn != n) {
      CSI_STALL(ESP32P4_DBG_NET, "jpeg send stall n=%u sent=%u ms=%u", (unsigned)n, (unsigned)sentn,
                (unsigned)dt);
      break;
    }
    if (mjpeg_sock_send(client, (const uint8_t *)"\r\n", 2, 80) != 2) break;
    _sent++;
    _last_jpeg = n;
    CSI_DBG(ESP32P4_DBG_NET, "jpeg n=%u ms=%u sent=%u", (unsigned)n, (unsigned)dt, (unsigned)_sent);
  }
  CSI_EVT(ESP32P4_DBG_STREAM, "client close sent=%u", (unsigned)_sent);
  client.stop();
}

void ESP32P4_MjpegServer::handleStatus() {
  bool hm = false, vf = false, aec = true, agc = true;
  uint16_t exp = 0, gain = 0, ceil = 0;
  _cam->getHMirror(&hm);
  _cam->getVFlip(&vf);
  _cam->getAEC(&aec);
  _cam->getAGC(&agc);
  _cam->getExposure(&exp);
  _cam->getGain(&gain);
  _cam->getGainCeiling(&ceil);

  // While Smart AE owns exposure/gain, report sensor AEC/AGC as off in UI.
  if (_smart_ae.enabled()) {
    aec = false;
    agc = false;
    exp = _smart_ae.lastExposure();
    gain = _smart_ae.lastGain();
  }

  // Keep off the HTTP task stack — a 4KB local here caused Stack protection fault on p4cam_http.
  static char buf[3584];
  static char qr_esc[ESP32P4_QR_MAX_PAYLOAD * 2];
  static char det_esc[384];
  static char note_esc[192];
  qr_esc[0] = '\0';
  det_esc[0] = '\0';
  note_esc[0] = '\0';
  if (_qr.on && _qr.payload[0]) {
    size_t o = 0;
    for (size_t i = 0; _qr.payload[i] && o + 6 < sizeof(qr_esc); i++) {
      char c = _qr.payload[i];
      if (c == '\\' || c == '"') {
        qr_esc[o++] = '\\';
        qr_esc[o++] = c;
      } else if ((uint8_t)c < 0x20) {
        o += (size_t)snprintf(qr_esc + o, sizeof(qr_esc) - o, "\\u%04x", (unsigned)(uint8_t)c);
      } else {
        qr_esc[o++] = c;
      }
    }
    qr_esc[o] = '\0';
  }
  if (_det.on && _det.summary[0]) {
    size_t o = 0;
    for (size_t i = 0; _det.summary[i] && o + 2 < sizeof(det_esc); i++) {
      char c = _det.summary[i];
      if (c == '\\' || c == '"') {
        det_esc[o++] = '\\';
        det_esc[o++] = c;
      } else if ((uint8_t)c >= 0x20) {
        det_esc[o++] = c;
      }
    }
    det_esc[o] = '\0';
  }
  if (_preview_note[0]) {
    size_t o = 0;
    for (size_t i = 0; _preview_note[i] && o + 2 < sizeof(note_esc); i++) {
      char c = _preview_note[i];
      if (c == '\\' || c == '"') {
        note_esc[o++] = '\\';
        note_esc[o++] = c;
      } else if ((uint8_t)c >= 0x20) {
        note_esc[o++] = c;
      }
    }
    note_esc[o] = '\0';
  }
  snprintf(buf, sizeof(buf),
           "{\"sensor\":\"%s\",\"framesize\":%u,\"out_w\":%u,\"out_h\":%u,"
           "\"w\":%u,\"h\":%u,\"native_w\":%u,\"native_h\":%u,"
           "\"quality\":%u,\"frameskip\":%u,\"jpeg\":%u,\"encode_ms\":%u,"
           "\"sent\":%u,\"dropped\":%u,\"psram\":%u,"
           "\"phase\":%u,\"age_ms\":%u,\"cap_fail\":%u,\"jpg_fail\":%u,"
           "\"csi_done\":%u,\"csi_drop\":%u,\"jpg_busy\":%u,\"ppa_to\":%u,"
           "\"control_port\":%u,\"stream_port\":%u,"
           "\"hmirror\":%u,\"vflip\":%u,\"aec\":%u,\"agc\":%u,"
           "\"aec_value\":%u,\"agc_gain\":%u,\"gainceiling\":%u,\"colorbar\":%u,"
           "\"smart_ae\":%u,\"smart_ae_ev\":%d,\"smart_ae_meter\":%.1f,\"smart_ae_hi\":%.3f,\"smart_ae_ms\":%u,"
           "\"isp_ready\":%u,\"isp_luma\":%.1f,\"isp_env\":%.1f,"
           "\"sd_ok\":%u,\"sd_folder\":\"%s\",\"saved\":%u,\"last_saved\":\"%s\","
           "\"video_ok\":%u,\"video_folder\":\"%s\",\"videos\":%u,\"last_video\":\"%s\","
           "\"recording\":%u,\"finalizing\":%u,\"rec_ms\":%u,\"rec_frames\":%u,\"mux_pct\":%u,\"rec_save_ok\":%d,"
           "\"mic_ok\":%u,\"mic_rate\":%u,\"mic_gain\":%u,\"mic_rms\":%.3f,\"mic_peak\":%.3f,"
           "\"cv_ok\":%u,\"cv_mode\":%u,\"cv_preset\":%u,"
           "\"cv_h_lo\":%u,\"cv_h_hi\":%u,\"cv_s_lo\":%u,\"cv_s_hi\":%u,"
           "\"cv_v_lo\":%u,\"cv_v_hi\":%u,\"cv_erode\":%u,\"cv_dilate\":%u,"
           "\"cv_min_area\":%u,\"cv_thr\":%u,\"cv_edge_lo\":%u,\"cv_edge_hi\":%u,"
           "\"cv_blobs\":%d,\"cv_tracks\":%d,\"cv_mask_px\":%d,\"cv_ms\":%d,\"cv_track_dist\":%u,"
           "\"face_ok\":%u,\"face_model\":%u,\"face_n\":%d,\"face_ms\":%d,\"face_feats\":%d,"
           "\"face_detect\":%u,\"face_recog\":%u,\"face_thr\":%u,\"face_lock\":%u,"
           "\"face_enroll_ok\":%d,\"face_enroll_id\":%d,\"face_enroll_got\":%d,\"face_enroll_need\":%d,"
           "\"face_roster\":\"%s\",\"face_db\":\"%s\","
           "\"qr_ok\":%u,\"qr_en\":%u,\"qr_n\":%d,\"qr_ms\":%d,\"qr_fmts\":%u,\"qr_fmt\":\"%s\",\"qr_payload\":\"%s\","
           "\"det_ok\":%u,\"det_en\":%u,\"det_model\":%u,\"det_n\":%d,\"det_ms\":%d,\"det_thr\":%u,\"det_summary\":\"%s\","
           "\"preview_note\":\"%s\"}\n",
           _cam->sensorName(), (unsigned)_framesize, (unsigned)_out_w, (unsigned)_out_h,
           (unsigned)_out_w, (unsigned)_out_h, (unsigned)_cam->width(), (unsigned)_cam->height(),
           (unsigned)_quality, (unsigned)_frame_skip, (unsigned)_last_jpeg, (unsigned)_encode_ms,
           (unsigned)_sent, (unsigned)_dropped, (unsigned)esp32p4_psram_free_size(),
           (unsigned)_work_phase, (unsigned)lastFrameAgeMs(), (unsigned)_cap_fail,
           (unsigned)_jpeg_fail, (unsigned)_cam->doneCount(), (unsigned)_cam->dropCount(),
           (unsigned)jpgBusyMask(), (unsigned)_ppa.timeoutCount(),
           (unsigned)_port, (unsigned)_stream_port, hm ? 1u : 0u, vf ? 1u : 0u, aec ? 1u : 0u,
           agc ? 1u : 0u, (unsigned)exp, (unsigned)gain, (unsigned)ceil,
           _cam->testPattern() ? 1u : 0u, _smart_ae.enabled() ? 1u : 0u, _smart_ae.evBias(),
           _smart_ae.lastMeter(), _smart_ae.lastHighlight(), (unsigned)_smart_ae.lastMs(),
           _cam->ispReady() ? 1u : 0u, _cam->ispLuma(), _cam->ispEnvLuma(),
           sdCaptureEnabled() ? 1u : 0u, _sd_folder,
           (unsigned)_saved, _last_saved, videoRecordEnabled() ? 1u : 0u, _video_folder,
           (unsigned)_videos, _last_video, _recording ? 1u : 0u, _rec_finalizing ? 1u : 0u,
           (unsigned)(_recording && _h264 ? _h264->recordElapsedMs() : 0),
           (unsigned)(_recording && _h264 ? _h264->framesEncoded() : 0),
           (unsigned)(_h264 ? _h264->muxProgress() : 0), (int)_rec_save_ok,
           micEnabled() ? 1u : 0u, (unsigned)(_mic ? _mic->sampleRate() : 0),
           (unsigned)(_mic ? _mic->gain() : 0), _mic ? _mic->rms() : 0.0f,
           _mic ? _mic->peak() : 0.0f, _cv_on ? 1u : 0u, (unsigned)_cv.mode,
           (unsigned)_cv.preset, (unsigned)_cv.lo.h, (unsigned)_cv.hi.h, (unsigned)_cv.lo.s,
           (unsigned)_cv.hi.s, (unsigned)_cv.lo.v, (unsigned)_cv.hi.v, (unsigned)_cv.erode_it,
           (unsigned)_cv.dilate_it, (unsigned)_cv.min_area, (unsigned)_cv.thr,
           (unsigned)_cv.edge_lo, (unsigned)_cv.edge_hi, (int)_cv.blobs, (int)_cv.tracks,
           (int)_cv.mask_px, (int)_cv.proc_ms, (unsigned)_cv.track_dist,
           _face.on ? 1u : 0u, (unsigned)_face.model, (int)_face.faces, (int)_face.ms,
           (int)_face.feats, _face.detect_en ? 1u : 0u, _face.recog_en ? 1u : 0u,
           (unsigned)_face.thr_pct, faceResLocked() ? 1u : 0u, (int)_face.enroll_ok,
           (int)_face.enroll_id, (int)_face.enroll_got, (int)_face.enroll_need, _face.roster,
           _face.db_path[0] ? _face.db_path : "/sdcard/face/face.db",
           _qr.on ? 1u : 0u, _qr.scan_en ? 1u : 0u, (int)_qr.codes, (int)_qr.ms,
           (unsigned)(_qr.formats ? _qr.formats : ESP32P4_Qr::defaultFormats()),
           _qr.format_name[0] ? _qr.format_name : "", qr_esc,
           _det.on ? 1u : 0u, _det.detect_en ? 1u : 0u, (unsigned)_det.model, (int)_det.objs,
           (int)_det.ms, (unsigned)_det.thr_pct, det_esc, note_esc);
  _http->send(200, "application/json", buf);
}

void ESP32P4_MjpegServer::handleAudio() {
  if (!micEnabled()) {
    _http->send(503, "application/json", "{\"ok\":0,\"error\":\"mic not enabled\"}");
    return;
  }
  int8_t wave[ESP32P4_MIC_WAVE_BINS];
  _mic->copyWave(wave, ESP32P4_MIC_WAVE_BINS);
  char buf[512];
  size_t o = 0;
  o += (size_t)snprintf(buf + o, sizeof(buf) - o,
                        "{\"ok\":1,\"rate\":%d,\"gain\":%d,\"rms\":%.4f,\"peak\":%.4f,\"wave\":[",
                        _mic->sampleRate(), _mic->gain(), (double)_mic->rms(),
                        (double)_mic->peak());
  for (int i = 0; i < ESP32P4_MIC_WAVE_BINS && o + 8 < sizeof(buf); i++) {
    o += (size_t)snprintf(buf + o, sizeof(buf) - o, "%s%d", i ? "," : "", (int)wave[i]);
  }
  if (o + 3 < sizeof(buf)) {
    buf[o++] = ']';
    buf[o++] = '}';
    buf[o] = '\0';
  }
  _http->send(200, "application/json", buf);
}

void ESP32P4_MjpegServer::handleAudioStream(bool wav_header) {
  if (!micEnabled()) {
    _audio_http->send(503, "text/plain", "mic not enabled");
    return;
  }
  WiFiClient client = _audio_http->client();
  // Nagle on: one ~40 ms PCM packet, not 150 tiny TCP/SDIO transactions/s.
  client.setNoDelay(false);
  client.setTimeout(50);
  if (_mic) _mic->clearStream();

  const uint32_t rate = (uint32_t)(_mic ? _mic->sampleRate() : 16000);
  char http[256];
  int hlen = snprintf(http, sizeof(http),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: %s\r\n"
                      "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                      "Pragma: no-cache\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Connection: close\r\n\r\n",
                      wav_header ? "audio/wav" : "application/octet-stream");
  if (hlen < 0 || mjpeg_sock_send(client, (const uint8_t *)http, (size_t)hlen, 300) != (size_t)hlen) {
    client.stop();
    return;
  }

  if (wav_header) {
    const uint32_t data_bytes = 0x7fffffffUL;
    uint8_t wav[44];
    memcpy(wav + 0, "RIFF", 4);
    wav[4] = (uint8_t)(data_bytes & 0xff);
    wav[5] = (uint8_t)((data_bytes >> 8) & 0xff);
    wav[6] = (uint8_t)((data_bytes >> 16) & 0xff);
    wav[7] = (uint8_t)((data_bytes >> 24) & 0xff);
    memcpy(wav + 8, "WAVEfmt ", 8);
    wav[16] = 16;
    wav[17] = 0;
    wav[18] = 0;
    wav[19] = 0;
    wav[20] = 1;
    wav[21] = 0;
    wav[22] = 1;
    wav[23] = 0;
    wav[24] = (uint8_t)(rate & 0xff);
    wav[25] = (uint8_t)((rate >> 8) & 0xff);
    wav[26] = (uint8_t)((rate >> 16) & 0xff);
    wav[27] = (uint8_t)((rate >> 24) & 0xff);
    const uint32_t byte_rate = rate * 2;
    wav[28] = (uint8_t)(byte_rate & 0xff);
    wav[29] = (uint8_t)((byte_rate >> 8) & 0xff);
    wav[30] = (uint8_t)((byte_rate >> 16) & 0xff);
    wav[31] = (uint8_t)((byte_rate >> 24) & 0xff);
    wav[32] = 2;
    wav[33] = 0;
    wav[34] = 16;
    wav[35] = 0;
    memcpy(wav + 36, "data", 4);
    wav[40] = (uint8_t)(data_bytes & 0xff);
    wav[41] = (uint8_t)((data_bytes >> 8) & 0xff);
    wav[42] = (uint8_t)((data_bytes >> 16) & 0xff);
    wav[43] = (uint8_t)((data_bytes >> 24) & 0xff);
    if (mjpeg_sock_send(client, wav, sizeof(wav), 200) != sizeof(wav)) {
      client.stop();
      return;
    }
  }

  // ~40 ms of s16le @ mic rate (640 samples at 16 kHz). One send per burst.
  size_t batch = (size_t)rate / 25u;
  if (batch < 160) batch = 160;
  if (batch > 1280) batch = 1280;
  int16_t pcm[1280];
  while (client.connected() && _http_run && micEnabled()) {
    size_t got = 0;
    const uint32_t t0 = millis();
    while (got < batch && client.connected() && _http_run) {
      size_t n = _mic ? _mic->readStream(pcm + got, batch - got) : 0;
      if (n) {
        got += n;
        continue;
      }
      if (got && (millis() - t0) >= 25) break;
      if ((millis() - t0) >= 50) break;
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (!got) {
      vTaskDelay(1);
      continue;
    }
    const size_t bytes = got * sizeof(int16_t);
    const size_t sent = mjpeg_sock_send(client, (const uint8_t *)pcm, bytes, 80);
    if (sent == bytes) {
      vTaskDelay(1);
      continue;
    }
    if (!sent) {
      CSI_DBG(ESP32P4_DBG_AUDIO, "drop 40ms pcm - tcp congested");
      continue;
    }
    const size_t rest = bytes - sent;
    if (mjpeg_sock_send(client, (const uint8_t *)pcm + sent, rest, 200) != rest) break;
  }
  client.stop();
}

bool ESP32P4_MjpegServer::applyControl(const String &var, int val) {
  if (var == "debug") {
    ESP32P4_Debug::setMask((uint32_t)val, true);
    return true;
  }
  if (var == "debug_ms") {
    ESP32P4_Debug::setPeriodMs((uint32_t)val, true);
    return true;
  }
  if (var == "quality") {
    setQuality((uint8_t)val);
    _face.settings_dirty = true;
    return true;
  }
  if (var == "frameskip") {
    setFrameSkip((uint8_t)val);
    _face.settings_dirty = true;
    return true;
  }
  if (var == "framesize") {
    bool ok = setFramesize((uint8_t)val);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "hmirror") {
    bool ok = _cam->setHMirror(val != 0);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "vflip") {
    bool ok = _cam->setVFlip(val != 0);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "smart_ae") {
    return enableSmartAe(val != 0);
  }
  if (var == "smart_ae_ev") {
    _smart_ae.setEvBias(val);
    if (!_smart_ae.enabled() && _cam) (void)_cam->setAeEvBias(val);
    return true;
  }
  if (var == "aec") {
    if (val != 0) _smart_ae.setEnabled(false);
    bool ok = _cam->setAEC(val != 0);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "agc") {
    if (val != 0) _smart_ae.setEnabled(false);
    bool ok = _cam->setAGC(val != 0);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "aec_value") {
    if (_smart_ae.enabled()) return true;  // driven by Smart AE
    bool ok = _cam->setExposure((uint16_t)val);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "agc_gain") {
    if (_smart_ae.enabled()) return true;
    bool ok = _cam->setGain((uint16_t)val);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "gainceiling") {
    bool ok = _cam->setGainCeiling((uint16_t)val);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "colorbar" || var == "test_pattern") {
    bool ok = _cam->setTestPattern(val != 0);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "mic_gain") {
    if (!_mic || !_mic->ready()) return false;
    bool ok = _mic->setGain(val);
    if (ok) _face.settings_dirty = true;
    return ok;
  }
  if (var == "face_model") {
    _face.model = val;
    _face.model_req = true;
    _face.settings_dirty = true;
    if (faceResLocked()) {
      applyFaceForcedDims();
      _size_dirty = true;
    }
    return true;
  }
  if (var == "qr_en") {
    _qr.scan_en = val != 0;
    if (!_qr.scan_en) {
      _qr.codes = 0;
      _qr.ms = 0;
    }
    _qr.settings_dirty = true;
    return true;
  }
  if (var == "det_en") {
    _det.detect_en = val != 0;
    if (!_det.detect_en) {
      _det.objs = 0;
      _det.ms = 0;
      _det.summary[0] = '\0';
    }
    _det.settings_dirty = true;
    return true;
  }
  if (var == "det_model") {
    bool ok = false;
    if (_det.opt_n > 0) {
      for (int i = 0; i < _det.opt_n; i++) {
        if (val == _det.opt_value[i]) {
          ok = true;
          break;
        }
      }
    } else {
      ok = val >= 0 && val <= 9;
    }
    if (!ok) return false;
    _det.model = val;
    _det.model_req = true;
    _det.settings_dirty = true;
    return true;
  }
  if (var == "det_thr") {
    if (val < 5) val = 5;
    if (val > 95) val = 95;
    _det.thr_pct = val;
    _det.thr_req = true;
    _det.settings_dirty = true;
    return true;
  }
  if (var == "qr_fmts") {
    uint32_t mask = (uint32_t)val & ESP32P4_Qr::defaultFormats();
    if (!mask) mask = ESP32P4_Qr::defaultFormats();
    _qr.formats = mask;
    _qr.settings_dirty = true;
    return true;
  }
  if (var == "face_detect") {
    _face.detect_en = val != 0;
    if (!_face.detect_en) {
      _face.recog_en = false;
      _face.enroll_req = false;
      _face.enroll_cancel = true;
      if (_face.enroll_ok == 2) _face.enroll_ok = 0;
    }
    applyFramesizeDims();
    _size_dirty = true;
    _face.settings_dirty = true;
    return true;
  }
  if (var == "face_recog") {
    // Recognition implies detection. Never touch face_model (that caused ESPDet→MSR reset).
    if (val != 0) {
      _face.detect_en = true;
      _face.recog_en = true;
      // Mutual exclusion: cancel any pending/in-progress enroll.
      _face.enroll_req = false;
      _face.enroll_cancel = true;
      if (_face.enroll_ok == 2) _face.enroll_ok = 0;
    } else {
      _face.recog_en = false;
    }
    applyFramesizeDims();
    _size_dirty = true;
    _face.settings_dirty = true;
    return true;
  }
  if (var == "face_thr") {
    if (val < 10) val = 10;
    if (val > 95) val = 95;
    _face.thr_pct = val;
    _face.thr_req = true;
    _face.settings_dirty = true;
    return true;
  }
  if (var == "face_enroll") {
    // Keep recog_en as the user left it — FaceAi skips FR only while enroll is in progress.
    _face.detect_en = true;
    _face.enroll_cancel = false;
    _face.enroll_req = true;
    _face.enroll_ok = 2;
    if (_face.model != 0) {
      _face.model = 0;
      _face.model_req = true;
    }
    applyFramesizeDims();
    _size_dirty = true;
    return true;
  }
  if (var == "face_clear") {
    _face.clear_req = true;
    return true;
  }
  if (var == "face_delete") {
    _face.delete_id = val;
    _face.delete_req = true;
    return true;
  }
  if (!_cv_on) return false;
  if (var == "cv_mode") {
    _cv.mode = (uint8_t)val;
    return true;
  }
  if (var == "cv_preset") {
    ESP32P4_CvDash::applyPreset(_cv, (uint8_t)val);
    return true;
  }
  if (var == "cv_h_lo") {
    _cv.lo.h = (uint8_t)val;
    _cv.preset = ESP32P4_CV_PRESET_CUSTOM;
    return true;
  }
  if (var == "cv_h_hi") {
    _cv.hi.h = (uint8_t)val;
    _cv.preset = ESP32P4_CV_PRESET_CUSTOM;
    return true;
  }
  if (var == "cv_s_lo") {
    _cv.lo.s = (uint8_t)val;
    _cv.preset = ESP32P4_CV_PRESET_CUSTOM;
    return true;
  }
  if (var == "cv_s_hi") {
    _cv.hi.s = (uint8_t)val;
    _cv.preset = ESP32P4_CV_PRESET_CUSTOM;
    return true;
  }
  if (var == "cv_v_lo") {
    _cv.lo.v = (uint8_t)val;
    _cv.preset = ESP32P4_CV_PRESET_CUSTOM;
    return true;
  }
  if (var == "cv_v_hi") {
    _cv.hi.v = (uint8_t)val;
    _cv.preset = ESP32P4_CV_PRESET_CUSTOM;
    return true;
  }
  if (var == "cv_erode") {
    _cv.erode_it = (uint8_t)val;
    return true;
  }
  if (var == "cv_dilate") {
    _cv.dilate_it = (uint8_t)val;
    return true;
  }
  if (var == "cv_min_area") {
    _cv.min_area = (uint16_t)val;
    return true;
  }
  if (var == "cv_thr") {
    _cv.thr = (uint8_t)val;
    return true;
  }
  if (var == "cv_edge_lo") {
    _cv.edge_lo = (uint8_t)val;
    return true;
  }
  if (var == "cv_edge_hi") {
    _cv.edge_hi = (uint8_t)val;
    return true;
  }
  if (var == "cv_track_dist") {
    _cv.track_dist = (uint8_t)val;
    return true;
  }
  return false;
}

void ESP32P4_MjpegServer::handleDebug() {
  ESP32P4_Debug::ensure();
  char bits[192];
  ESP32P4_Debug::namesJson(bits, sizeof(bits));
  static char buf[512];
  snprintf(buf, sizeof(buf),
           "{\"ok\":1,\"app\":\"%s\",\"mask\":%u,\"ms\":%u,\"stall\":\"%s\",\"bits\":%s}\n",
           ESP32P4_Debug::app(), (unsigned)ESP32P4_Debug::mask(),
           (unsigned)ESP32P4_Debug::periodMs(), ESP32P4_Debug::lastStall(), bits);
  _http->send(200, "application/json", buf);
}

void ESP32P4_MjpegServer::handleControl() {
  if (!_http->hasArg("var") || !_http->hasArg("val")) {
    _http->send(400, "text/plain", "need var & val");
    return;
  }
  String var = _http->arg("var");
  String sval = _http->arg("val");
  if (var == "face_enroll_name") {
    memset(_face.enroll_name, 0, sizeof(_face.enroll_name));
    size_t n = sval.length();
    if (n >= sizeof(_face.enroll_name)) n = sizeof(_face.enroll_name) - 1;
    for (size_t i = 0; i < n; i++) {
      char c = sval[i];
      if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-' || c == '.') {
        _face.enroll_name[i] = c;
      } else {
        _face.enroll_name[i] = '_';
      }
    }
    _http->send(200, "text/plain", "1");
    return;
  }
  if (var == "face_delete_name") {
    memset(_face.delete_name, 0, sizeof(_face.delete_name));
    size_t n = sval.length();
    if (n >= sizeof(_face.delete_name)) n = sizeof(_face.delete_name) - 1;
    for (size_t i = 0; i < n; i++) {
      char c = sval[i];
      if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-' || c == '.') {
        _face.delete_name[i] = c;
      } else {
        _face.delete_name[i] = '_';
      }
    }
    _face.delete_name_req = true;
    _http->send(200, "text/plain", "1");
    return;
  }
  int val = sval.toInt();
  if (!applyControl(var, val)) {
    _http->send(400, "text/plain", "unsupported or failed");
    return;
  }
  _http->send(200, "text/plain", "1");
}
