#include "stream/ESP32P4_Mjpeg.h"

#include "mem/ESP32P4_Psram.h"

#include <esp_wifi.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<title>ESP32CSI_Vision</title>
<style>
:root{--bg:#0b1220;--card:#141c2b;--fg:#e8eef7;--muted:#8b9bb4;--acc:#3d8bfd;--line:#243044;--pad:clamp(8px,2.5vw,16px)}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
body{padding-bottom:env(safe-area-inset-bottom)}
header{position:sticky;top:0;z-index:5;padding:var(--pad) calc(var(--pad) + env(safe-area-inset-right)) var(--pad) calc(var(--pad) + env(safe-area-inset-left));background:rgba(11,18,32,.92);backdrop-filter:blur(8px);border-bottom:1px solid var(--line);display:flex;flex-wrap:wrap;gap:8px 14px;align-items:baseline}
header h1{margin:0;font-size:clamp(16px,4vw,20px);font-weight:650}
#meta{color:var(--muted);font-size:clamp(11px,2.8vw,12px);word-break:break-word;flex:1 1 200px}
main{display:grid;grid-template-columns:minmax(0,1.4fr) minmax(260px,360px);gap:var(--pad);padding:var(--pad);padding-left:calc(var(--pad) + env(safe-area-inset-left));padding-right:calc(var(--pad) + env(safe-area-inset-right));align-items:start}
@media (max-width:860px){main{grid-template-columns:1fr}}
.view,.panel{background:var(--card);border-radius:12px;padding:var(--pad);min-width:0}
.view img{width:100%;max-height:min(70vh,720px);height:auto;object-fit:contain;border-radius:8px;background:#000;display:block}
.links{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
.links a{color:var(--acc);text-decoration:none;font-size:12px}
.panel{max-height:min(78vh,900px);overflow:auto;-webkit-overflow-scrolling:touch}
.panel h2{margin:16px 0 8px;font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.panel h2:first-child{margin-top:0}
.row{display:grid;grid-template-columns:1fr auto;gap:6px 10px;align-items:center;margin:10px 0}
.row label{font-size:13px}
.row .val{font-variant-numeric:tabular-nums;color:var(--muted);font-size:12px;min-width:2.5em;text-align:right}
.row .hint{grid-column:1/-1;color:var(--muted);font-size:11px}
.row.full{grid-template-columns:1fr}
input[type=range]{width:100%;grid-column:1/-1;accent-color:var(--acc)}
select,button{width:100%;background:#0f1726;color:var(--fg);border:1px solid #2b3b55;border-radius:8px;padding:10px 12px;font-size:14px}
.row select{grid-column:1/-1}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}
button{cursor:pointer;background:var(--acc);border:none;font-weight:600}
button.secondary{background:#243044}
.live{display:inline-flex;align-items:center;gap:6px;font-size:11px;color:#3dd68c}
.live i{width:7px;height:7px;border-radius:50%;background:#3dd68c;display:inline-block;animation:pulse 1.2s infinite}
@keyframes pulse{50%{opacity:.35}}
.na{opacity:.5}
#toast{position:fixed;left:50%;bottom:calc(12px + env(safe-area-inset-bottom));transform:translateX(-50%);background:#102033;border:1px solid #2b3b55;padding:8px 14px;border-radius:999px;display:none;font-size:12px;z-index:20;max-width:90vw;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
</style>
</head>
<body>
<header>
  <h1>ESP32CSI_Vision</h1>
  <span class="live"><i></i>live</span>
  <div id="meta">connecting…</div>
</header>
<main>
  <section class="view">
    <img id="stream" alt="live stream"/>
    <div class="links">
      <a id="a_stream" href="#" target="_blank">MJPEG stream</a>
      <a href="/jpg" target="_blank">/jpg</a>
      <a href="/capture" target="_blank">/capture</a>
      <a href="/status" target="_blank">/status</a>
    </div>
  </section>
  <section class="panel">
    <h2>Stream (applies instantly)</h2>
    <div class="row full"><label>Resolution</label>
      <select id="framesize">
        <option value="0">SVGA 800×640 (native)</option>
        <option value="1">VGA 640×480</option>
        <option value="2">HVGA 480×320</option>
        <option value="3">QVGA 320×240</option>
        <option value="4">QQVGA 160×120</option>
      </select>
      <div class="hint">Live PPA scale. Lower = smoother. Sensor stays native CSI size.</div>
    </div>
    <div class="row"><label>JPEG quality</label><span class="val" id="v_quality">35</span>
      <input id="quality" type="range" min="4" max="63" value="35"/>
    </div>
    <div class="row"><label>Frame skip</label><span class="val" id="v_frameskip">0</span>
      <input id="frameskip" type="range" min="0" max="4" value="0"/>
    </div>
    <div class="btns">
      <button id="btn_reconnect" class="secondary" type="button">Reconnect</button>
      <button id="btn_snap" type="button">Snapshot</button>
    </div>

    <h2>CSI sensor (OV5647)</h2>
    <div class="row full"><label>H-Mirror</label>
      <select id="hmirror"><option value="0">Off</option><option value="1">On</option></select>
    </div>
    <div class="row full"><label>V-Flip</label>
      <select id="vflip"><option value="0">Off</option><option value="1">On</option></select>
    </div>
    <div class="row full"><label>AEC (auto exposure)</label>
      <select id="aec"><option value="1">Auto</option><option value="0">Manual</option></select>
    </div>
    <div class="row full"><label>AGC (auto gain)</label>
      <select id="agc"><option value="1">Auto</option><option value="0">Manual</option></select>
    </div>
    <div class="row"><label>Exposure</label><span class="val" id="v_aec_value">100</span>
      <input id="aec_value" type="range" min="1" max="1500" value="100"/>
    </div>
    <div class="row"><label>Gain</label><span class="val" id="v_agc_gain">16</span>
      <input id="agc_gain" type="range" min="0" max="1023" value="16"/>
    </div>
    <div class="row"><label>Gain ceiling</label><span class="val" id="v_gainceiling">248</span>
      <input id="gainceiling" type="range" min="16" max="1023" value="248"/>
    </div>
    <div class="row full"><label>Test pattern</label>
      <select id="colorbar"><option value="0">Off</option><option value="1">On</option></select>
    </div>

    <h2 class="na">Not on CSI</h2>
    <div class="row na full"><label>Brightness / contrast / AWB / effects</label>
      <div class="hint">DVP esp32-camera knobs do not apply to MIPI CSI.</div>
    </div>
  </section>
</main>
<div id="toast"></div>
<script>
const stream=document.getElementById('stream');
const basePort=location.port?parseInt(location.port,10):80;
const streamPort=basePort+1;
const streamUrl=location.protocol+'//'+location.hostname+':'+streamPort+'/stream';
document.getElementById('a_stream').href=streamUrl;
let applying=false, debounceTimers={};

function toast(m){const t=document.getElementById('toast');t.textContent=m;t.style.display='block';clearTimeout(t._t);t._t=setTimeout(()=>t.style.display='none',900)}

async function control(varName,val){
  applying=true;
  try{
    const ctrl=new AbortController();
    const timer=setTimeout(()=>ctrl.abort(),4000);
    const r=await fetch('/control?var='+encodeURIComponent(varName)+'&val='+encodeURIComponent(val)+'&_='+Date.now(),{cache:'no-store',signal:ctrl.signal});
    clearTimeout(timer);
    if(!r.ok){toast('failed: '+varName);return}
    toast(varName+'='+val);
    refreshMetaOnly();
  }catch(e){toast(e.name==='AbortError'?'timeout':'network error')}
  finally{applying=false}
}

function bindRange(id,varName){
  const el=document.getElementById(id), lab=document.getElementById('v_'+id);
  const sync=()=>{if(lab)lab.textContent=el.value};
  el.addEventListener('input',()=>{
    sync();
    clearTimeout(debounceTimers[id]);
    debounceTimers[id]=setTimeout(()=>control(varName,el.value),50);
  });
  sync();
}
function bindSelect(id,varName){
  document.getElementById(id).addEventListener('change',e=>control(varName,e.target.value));
}
bindRange('quality','quality');
bindRange('frameskip','frameskip');
bindRange('aec_value','aec_value');
bindRange('agc_gain','agc_gain');
bindRange('gainceiling','gainceiling');
bindSelect('framesize','framesize');
bindSelect('hmirror','hmirror');
bindSelect('vflip','vflip');
bindSelect('aec','aec');
bindSelect('agc','agc');
bindSelect('colorbar','colorbar');

document.getElementById('btn_reconnect').onclick=()=>{stream.src=streamUrl+'?ts='+Date.now()};
document.getElementById('btn_snap').onclick=()=>{
  // Same-origin snapshot on control port (not blocked by MJPEG).
  window.open('/capture?ts='+Date.now(),'_blank');
};

function fillForm(s){
  const set=(id,v)=>{const el=document.getElementById(id); if(!el||v===undefined||v===null)return; el.value=String(v); if(el.type==='range') el.dispatchEvent(new Event('input'))};
  set('framesize',s.framesize); set('quality',s.quality); set('frameskip',s.frameskip);
  set('hmirror',s.hmirror); set('vflip',s.vflip); set('aec',s.aec); set('agc',s.agc);
  set('aec_value',s.aec_value); set('agc_gain',s.agc_gain); set('gainceiling',s.gainceiling);
  set('colorbar',s.colorbar);
}
function metaText(s){
  return (s.sensor||'?')+' · out '+s.out_w+'x'+s.out_h+' · q'+s.quality+' · skip'+s.frameskip+
    ' · '+s.jpeg+'B · '+s.encode_ms+'ms · sent '+s.sent;
}
async function refreshMetaOnly(){
  try{
    const s=await (await fetch('/status?_='+Date.now(),{cache:'no-store'})).json();
    document.getElementById('meta').textContent=metaText(s);
  }catch(e){}
}
async function boot(){
  try{
    const s=await (await fetch('/status?_='+Date.now(),{cache:'no-store'})).json();
    fillForm(s);
    document.getElementById('meta').textContent=metaText(s);
  }catch(e){document.getElementById('meta').textContent='status error — is port '+basePort+' free?'}
  // MJPEG on port+1 so /control on this port stays responsive.
  stream.src=streamUrl+'?ts='+Date.now();
}
boot();
setInterval(()=>{ if(!applying) refreshMetaOnly(); },3000);
</script>
</body>
</html>
)HTML";

static void framesizeToWH(uint8_t fs, uint16_t *w, uint16_t *h) {
  switch (fs) {
    case ESP32P4_STREAM_VGA:
      *w = 640;
      *h = 480;
      break;
    case ESP32P4_STREAM_HVGA:
      *w = 480;
      *h = 320;
      break;
    case ESP32P4_STREAM_QVGA:
      *w = 320;
      *h = 240;
      break;
    case ESP32P4_STREAM_QQVGA:
      *w = 160;
      *h = 120;
      break;
    case ESP32P4_STREAM_SVGA:
    default:
      *w = 800;
      *h = 640;
      break;
  }
}

void ESP32P4_MjpegServer::applyFramesizeDims() {
  uint16_t w = 800, h = 640;
  framesizeToWH(_framesize, &w, &h);
  if (_cam) {
    if (w > _cam->width()) w = _cam->width();
    if (h > _cam->height()) h = _cam->height();
  }
  _out_w = w;
  _out_h = h;
}

bool ESP32P4_MjpegServer::setFramesize(uint8_t fs) {
  if (fs > ESP32P4_STREAM_QQVGA) return false;
  _framesize = fs;
  applyFramesizeDims();
  return true;
}

bool ESP32P4_MjpegServer::begin(ESP32P4_Camera *cam, uint16_t port, uint8_t quality) {
  if (!cam) return false;
  end();
  _cam = cam;
  _port = port;
  _stream_port = (uint16_t)(port + 1);
  _quality = quality < 4 ? 4 : (quality > 63 ? 63 : quality);
  if (!_jpeg.begin(cam->width(), cam->height(), _quality)) return false;
  _ppa.begin();

  _framesize = ESP32P4_STREAM_SVGA;
  applyFramesizeDims();

  _jpg_cap = 220 * 1024;
  for (int i = 0; i < 2; i++) {
    _jpg_buf[i] = (uint8_t *)esp32p4_psram_alloc(_jpg_cap);
    if (!_jpg_buf[i]) {
      end();
      return false;
    }
    _jpg_len[i] = 0;
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
  if (!_frame_sem || !_jpg_mutex) {
    end();
    return false;
  }

  // Control / UI server — never hosts blocking MJPEG.
  _http = new WebServer(_port);
  _http->on("/", HTTP_GET, [this]() { handleRoot(); });
  _http->on("/jpg", HTTP_GET, [this]() { handleJpg(); });
  _http->on("/capture", HTTP_GET, [this]() { handleCapture(); });
  _http->on("/status", HTTP_GET, [this]() { handleStatus(); });
  _http->on("/control", HTTP_GET, [this]() { handleControl(); });
  _http->begin();

  // Dedicated stream port so handleStream cannot block /control.
  _stream_http = new WebServer(_stream_port);
  _stream_http->on("/stream", HTTP_GET, [this]() { handleStream(); });
  _stream_http->on("/", HTTP_GET, [this]() {
    _stream_http->send(200, "text/plain", "MJPEG on /stream — open UI on control port");
  });
  _stream_http->begin();

  esp_wifi_set_ps(WIFI_PS_NONE);
  if (!startWorker()) return false;
  return startHttpTasks();
}

void ESP32P4_MjpegServer::loop() {
  // HTTP is handled in FreeRTOS tasks; keep for sketch compatibility.
  delay(1);
}

void ESP32P4_MjpegServer::end() {
  stopHttpTasks();
  stopWorker();
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
  _jpeg.end();
  _ppa.end();
  for (int i = 0; i < 2; i++) {
    esp32p4_psram_free(_jpg_buf[i]);
    _jpg_buf[i] = nullptr;
    _jpg_len[i] = 0;
  }
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
}

void ESP32P4_MjpegServer::setQuality(uint8_t q) {
  _quality = q < 4 ? 4 : (q > 63 ? 63 : q);
  _jpeg.setQuality(_quality);
}

void ESP32P4_MjpegServer::setFrameSkip(uint8_t skip) {
  _frame_skip = skip > 8 ? 8 : skip;
}

bool ESP32P4_MjpegServer::startHttpTasks() {
  _http_run = true;
  // Higher priority than stream so /control stays snappy.
  if (xTaskCreatePinnedToCore(controlHttpThunk, "p4cam_http", 6144, this, 6, &_control_task, 0) !=
      pdPASS) {
    return false;
  }
  if (xTaskCreatePinnedToCore(streamHttpThunk, "p4cam_strm", 6144, this, 4, &_stream_task, 0) !=
      pdPASS) {
    return false;
  }
  return true;
}

void ESP32P4_MjpegServer::stopHttpTasks() {
  _http_run = false;
  for (int i = 0; i < 50; i++) {
    if (!_control_task && !_stream_task) break;
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
}

void ESP32P4_MjpegServer::controlHttpThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->controlHttpLoop();
}

void ESP32P4_MjpegServer::streamHttpThunk(void *arg) {
  static_cast<ESP32P4_MjpegServer *>(arg)->streamHttpLoop();
}

void ESP32P4_MjpegServer::controlHttpLoop() {
  while (_http_run) {
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

bool ESP32P4_MjpegServer::startWorker() {
  if (_worker) return true;
  _worker_run = true;
  BaseType_t ok = xTaskCreatePinnedToCore(workerThunk, "p4cam_jpg", 8192, this, 5, &_worker, 1);
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
  while (_worker_run) {
    camera_fb_t *fb = _cam->capture(80);
    if (!fb) {
      vTaskDelay(1);
      continue;
    }
    if (skip_left) {
      skip_left--;
      _cam->release(fb);
      _dropped++;
      continue;
    }
    skip_left = _frame_skip;

    const uint16_t ow = _out_w;
    const uint16_t oh = _out_h;
    const uint8_t *rgb = fb->buf;
    uint16_t ew = fb->width;
    uint16_t eh = fb->height;

    if (ow != fb->width || oh != fb->height) {
      if (!_ppa.scale(fb, _scale_buf, _scale_cap, ow, oh)) {
        _cam->release(fb);
        _dropped++;
        continue;
      }
      rgb = _scale_buf;
      ew = ow;
      eh = oh;
    }

    const int i = _enc_idx;
    const uint32_t t0 = millis();
    size_t n = _jpeg.encode(rgb, ew, eh, _jpg_buf[i], _jpg_cap);
    _encode_ms = millis() - t0;
    _cam->release(fb);
    if (!n) {
      _dropped++;
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
  }
  _worker = nullptr;
  vTaskDelete(nullptr);
}

void ESP32P4_MjpegServer::handleRoot() {
  _http->send_P(200, "text/html", INDEX_HTML);
}

void ESP32P4_MjpegServer::sendJpeg(WebServer *srv) {
  if (!srv) return;
  int idx = _ready_idx;
  size_t n = 0;
  if (_jpg_mutex) xSemaphoreTake(_jpg_mutex, pdMS_TO_TICKS(50));
  idx = _ready_idx;
  if (idx >= 0) n = _jpg_len[idx];
  // Copy length under lock; buffer itself is stable until next overwrite of same slot.
  // Use the buffer that is NOT currently being encoded (_enc_idx is write target).
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

void ESP32P4_MjpegServer::handleStream() {
  WiFiClient client = _stream_http->client();
  client.setNoDelay(true);
  client.print(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
      "Cache-Control: no-cache, no-store, must-revalidate\r\n"
      "Pragma: no-cache\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Connection: close\r\n\r\n");

  uint32_t last_seq = 0;
  while (client.connected() && _http_run) {
    if (_frame_seq == last_seq) {
      if (xSemaphoreTake(_frame_sem, pdMS_TO_TICKS(200)) != pdTRUE) {
        vTaskDelay(1);
        continue;
      }
    }
    last_seq = _frame_seq;
    int idx = _ready_idx;
    if (idx < 0) continue;
    size_t n = _jpg_len[idx];
    if (!n) continue;

    if (!client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                       (unsigned)n)) {
      break;
    }
    size_t off = 0;
    while (off < n) {
      if (!client.connected()) break;
      size_t chunk = n - off;
      if (chunk > 2048) chunk = 2048;
      size_t w = client.write(_jpg_buf[idx] + off, chunk);
      if (!w) {
        vTaskDelay(1);
        break;
      }
      off += w;
    }
    if (off != n) break;
    if (!client.print("\r\n")) break;
    _sent++;
    _last_jpeg = n;
  }
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

  char buf[700];
  snprintf(buf, sizeof(buf),
           "{\"sensor\":\"%s\",\"framesize\":%u,\"out_w\":%u,\"out_h\":%u,"
           "\"w\":%u,\"h\":%u,\"native_w\":%u,\"native_h\":%u,"
           "\"quality\":%u,\"frameskip\":%u,\"jpeg\":%u,\"encode_ms\":%u,"
           "\"sent\":%u,\"dropped\":%u,\"psram\":%u,"
           "\"control_port\":%u,\"stream_port\":%u,"
           "\"hmirror\":%u,\"vflip\":%u,\"aec\":%u,\"agc\":%u,"
           "\"aec_value\":%u,\"agc_gain\":%u,\"gainceiling\":%u,\"colorbar\":%u}\n",
           _cam->sensorName(), (unsigned)_framesize, (unsigned)_out_w, (unsigned)_out_h,
           (unsigned)_out_w, (unsigned)_out_h, (unsigned)_cam->width(), (unsigned)_cam->height(),
           (unsigned)_quality, (unsigned)_frame_skip, (unsigned)_last_jpeg, (unsigned)_encode_ms,
           (unsigned)_sent, (unsigned)_dropped, (unsigned)esp32p4_psram_free_size(),
           (unsigned)_port, (unsigned)_stream_port, hm ? 1u : 0u, vf ? 1u : 0u, aec ? 1u : 0u,
           agc ? 1u : 0u, (unsigned)exp, (unsigned)gain, (unsigned)ceil,
           _cam->testPattern() ? 1u : 0u);
  _http->send(200, "application/json", buf);
}

bool ESP32P4_MjpegServer::applyControl(const String &var, int val) {
  if (var == "quality") {
    setQuality((uint8_t)val);
    return true;
  }
  if (var == "frameskip") {
    setFrameSkip((uint8_t)val);
    return true;
  }
  if (var == "framesize") return setFramesize((uint8_t)val);
  if (var == "hmirror") return _cam->setHMirror(val != 0);
  if (var == "vflip") return _cam->setVFlip(val != 0);
  if (var == "aec") return _cam->setAEC(val != 0);
  if (var == "agc") return _cam->setAGC(val != 0);
  if (var == "aec_value") return _cam->setExposure((uint16_t)val);
  if (var == "agc_gain") return _cam->setGain((uint16_t)val);
  if (var == "gainceiling") return _cam->setGainCeiling((uint16_t)val);
  if (var == "colorbar" || var == "test_pattern") return _cam->setTestPattern(val != 0);
  return false;
}

void ESP32P4_MjpegServer::handleControl() {
  if (!_http->hasArg("var") || !_http->hasArg("val")) {
    _http->send(400, "text/plain", "need var & val");
    return;
  }
  String var = _http->arg("var");
  int val = _http->arg("val").toInt();
  if (!applyControl(var, val)) {
    _http->send(400, "text/plain", "unsupported or failed");
    return;
  }
  _http->send(200, "text/plain", "1");
}
