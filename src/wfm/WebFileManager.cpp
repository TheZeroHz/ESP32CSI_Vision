#include "WebFileManager.h"
#include "WfmUtil.h"
#include "WfmUi.h"

#include <esp_heap_caps.h>
#include <Network.h>
#include <WiFi.h>
#include <ETH.h>

using namespace wfm;


WebFileManager::WebFileManager(WfmStorage &primary) {
  addVolume(primary.label(), primary);
}

WebFileManager &WebFileManager::addVolume(const char *name, WfmStorage &storage) {
  if (_volCount >= WFM_MAX_VOLUMES || !name || !name[0]) return *this;
  // Replace if same name already registered (constructor + explicit add).
  for (int i = 0; i < _volCount; i++) {
    if (_vols[i].name && strcmp(_vols[i].name, name) == 0) {
      _vols[i].store = &storage;
      _multi = _volCount > 1;
      return *this;
    }
  }
  _vols[_volCount++] = {name, &storage};
  _multi = _volCount > 1;
  return *this;
}

WebFileManager &WebFileManager::setPorts(uint16_t uiPort, uint16_t filePort) {
  _uiPort = uiPort;
  _filePort = filePort;
  return *this;
}

WebFileManager &WebFileManager::setHomePort(uint16_t port) {
  _homePort = port;
  return *this;
}

WebFileManager &WebFileManager::setName(const char *name) {
  if (name && name[0]) _name = name;
  return *this;
}

WebFileManager &WebFileManager::setAuth(const char *username, const char *password) {
  _authUser = username;
  _authPass = password ? password : "";
  return *this;
}

bool WebFileManager::requireAuth(WebServer &srv) {
  if (!authEnabled()) return true;
  if (srv.authenticate(_authUser, _authPass)) return true;
  // JSON 401 — UI shows login (avoids browser Basic dialog).
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
  srv.send(401, "application/json", "{\"error\":\"auth required\",\"auth\":true}");
  return false;
}

const char *WebFileManager::volumeTypeOf(const char *name) {
  if (!name) return "vol";
  String n = name;
  n.toLowerCase();
  if (n.indexOf("sd") >= 0) return "sd";
  if (n.indexOf("little") >= 0 || n.indexOf("spiff") >= 0 || n.indexOf("ffat") >= 0 ||
      n.indexOf("flash") >= 0)
    return "flash";
  return "vol";
}

bool WebFileManager::anyMounted() const {
  for (int i = 0; i < _volCount; i++) {
    if (_vols[i].store && _vols[i].store->mounted()) return true;
  }
  return false;
}

String WebFileManager::makePublicPath(const WfmVolume &vol, const String &local) const {
  if (!_multi) return local;
  if (local == "/") return String("/") + vol.name;
  return String("/") + vol.name + local;
}

bool WebFileManager::resolve(const String &path, WfmStorage *&store, String &local,
                             bool *atVolumeRoot) const {
  store = nullptr;
  local = "/";
  if (atVolumeRoot) *atVolumeRoot = false;
  if (!path.length() || path[0] != '/' || path.indexOf("..") >= 0) return false;

  if (!_multi) {
    if (!_volCount || !_vols[0].store) return false;
    store = _vols[0].store;
    local = path;
    return pathOk(path);
  }

  // Multi: / or "" -> virtual root (caller lists volumes)
  if (path == "/") {
    if (atVolumeRoot) *atVolumeRoot = true;
    return true;
  }

  // /Name or /Name/rest
  String rest = path.substring(1);
  int slash = rest.indexOf('/');
  String volName = slash < 0 ? rest : rest.substring(0, slash);
  String rem = slash < 0 ? "/" : rest.substring(slash);
  if (!rem.length()) rem = "/";
  if (rem[0] != '/') rem = "/" + rem;

  for (int i = 0; i < _volCount; i++) {
    if (_vols[i].name && volName.equals(_vols[i].name)) {
      store = _vols[i].store;
      local = rem;
      if (atVolumeRoot) *atVolumeRoot = (rem == "/");
      return store && pathOk(local);
    }
  }
  return false;
}

void WebFileManager::sendJsonOk(WebServer &srv) { srv.send(200, "application/json", "{\"ok\":true}"); }

void WebFileManager::sendJsonErr(WebServer &srv, int code, const char *msg) {
  char buf[160];
  char esc[96];
  jsonEscapeTo(msg ? msg : "error", esc, sizeof(esc));
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", esc);
  srv.send(code, "application/json", buf);
}

bool WebFileManager::begin() {
  if (_started) return true;
  if (!anyMounted()) return false;

  _ui = new WebServer(_uiPort);
  _file = new WebServer(_filePort);
  if (!_ui || !_file) return false;

  bindRoutes();
  _ui->begin();
  _file->begin();
  _started = true;
  refreshUsageAsync();
  return true;
}

void WebFileManager::bindRoutes() {
  static const char *uiHdrs[] = {"Authorization"};
  _ui->collectHeaders(uiHdrs, 1);

  _ui->on("/", HTTP_GET, [this]() { handleRoot(); });  // public — login UI
  _ui->on("/api/ping", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handlePing();
  });
  _ui->on("/api/status", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleStatus();
  });
  _ui->on("/api/volumes", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleVolumes();
  });
  _ui->on("/api/list", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleList();
  });
  _ui->on("/api/text", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleText();
  });
  _ui->on("/api/text", HTTP_POST, [this]() {
    if (!requireAuth(*_ui)) return;
    handleTextSave();
  });
  _ui->on("/api/text/save", HTTP_POST, [this]() {
    if (!requireAuth(*_ui)) return;
    handleTextSave();
  });
  _ui->on("/api/details", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleDetails();
  });
  _ui->on("/api/delete", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleDelete();
  });
  _ui->on("/api/mkdir", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleMkdir();
  });
  _ui->on("/api/create", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleCreateFile();
  });
  _ui->on("/api/rename", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleRename();
  });
  _ui->on("/api/copy", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleCopy();
  });
  _ui->on("/api/move", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleMove();
  });
  _ui->on("/api/search", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleSearch();
  });
  // Same-origin upload (reliable). Optional mirror on :filePort.
  _ui->on(
      "/api/upload", HTTP_POST,
      [this]() {
        if (!requireAuth(*_ui)) return;
        handleUploadDone(*_ui);
      },
      [this]() { handleUploadStream(*_ui); });
  _ui->on("/view", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleView(*_ui);
  });
  _ui->on("/download", HTTP_GET, [this]() {
    if (!requireAuth(*_ui)) return;
    handleDownload(*_ui);
  });
  _ui->onNotFound([this]() { _ui->send(404, "text/plain", "Not found"); });

  static const char *hdrs[] = {"Range", "Authorization"};
  _file->collectHeaders(hdrs, 2);
  _file->enableCORS(true);
  _file->on("/view", HTTP_GET, [this]() {
    if (!requireAuth(*_file)) return;
    handleView(*_file);
  });
  _file->on("/download", HTTP_GET, [this]() {
    if (!requireAuth(*_file)) return;
    handleDownload(*_file);
  });
  _file->on("/api/upload", HTTP_OPTIONS, [this]() { handleUploadOptions(*_file); });
  _file->on(
      "/api/upload", HTTP_POST,
      [this]() {
        if (!requireAuth(*_file)) return;
        handleUploadDone(*_file);
      },
      [this]() { handleUploadStream(*_file); });
  _file->onNotFound([this]() {
    _file->sendHeader("Access-Control-Allow-Origin", "*");
    _file->send(404, "text/plain", "Not found");
  });
}

void WebFileManager::loop() {
  if (_started && _ui && !_fileTask && !_paused) _ui->handleClient();
}

void WebFileManager::fileTaskThunk(void *arg) {
  auto *self = static_cast<WebFileManager *>(arg);
  for (;;) {
    if (!self->_paused && self->_started) {
      if (self->_ui) self->_ui->handleClient();
      if (self->_file) self->_file->handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool WebFileManager::startFileTask(UBaseType_t priority, BaseType_t core, uint32_t stackWords) {
  if (!_started || _fileTask) return _fileTask != nullptr;
  return xTaskCreatePinnedToCore(fileTaskThunk, "wfm_file", stackWords, this, priority, &_fileTask,
                                 core) == pdPASS;
}

void WebFileManager::usageTaskThunk(void *arg) {
  auto *self = static_cast<WebFileManager *>(arg);
  self->_usageState = 1;
  uint64_t total = 0, used = 0;
  for (int i = 0; i < self->_volCount; i++) {
    WfmStorage *s = self->_vols[i].store;
    if (!s || !s->mounted()) continue;
    total += s->totalBytes();
    used += s->usedBytes();
  }
  if (total == 0) {
    self->_usageState = 3;
  } else {
    self->_total = total;
    self->_used = used;
    self->_usageState = 2;
  }
  vTaskDelete(nullptr);
}

void WebFileManager::refreshUsageAsync() {
  if (_usageState == 1) return;
  _usageState = 1;
  xTaskCreatePinnedToCore(usageTaskThunk, "wfm_usage", 8192, this, 1, nullptr, 0);
}

void WebFileManager::handleRoot() {
  noteBusy();
  _ui->setContentLength(CONTENT_LENGTH_UNKNOWN);
  _ui->send(200, "text/html; charset=utf-8", "");
  char boot[200];
  snprintf(boot, sizeof(boot),
           "<script>window.WFM_FILE_PORT=%u;window.WFM_HOME_PORT=%u;window.WFM_MULTI=%s;window.WFM_AUTH=%s;</script>",
           (unsigned)_filePort, (unsigned)_homePort, _multi ? "true" : "false",
           authEnabled() ? "true" : "false");
  _ui->sendContent(boot);
  _ui->sendContent_P(WFM_INDEX_HTML);
}

void WebFileManager::handlePing() { _ui->send(200, "application/json", "{\"ok\":true}"); }

void WebFileManager::handleVolumes() {
  char buf[512];
  size_t o = 0;
  o += snprintf(buf + o, sizeof(buf) - o, "{\"multi\":%s,\"volumes\":[", _multi ? "true" : "false");
  for (int i = 0; i < _volCount && o + 80 < sizeof(buf); i++) {
    char nesc[40];
    jsonEscapeTo(_vols[i].name, nesc, sizeof(nesc));
    bool m = _vols[i].store && _vols[i].store->mounted();
    o += snprintf(buf + o, sizeof(buf) - o, "%s{\"name\":\"%s\",\"mounted\":%s}", i ? "," : "", nesc,
                  m ? "true" : "false");
  }
  o += snprintf(buf + o, sizeof(buf) - o, "]}");
  _ui->send(200, "application/json", buf);
}

void WebFileManager::handleStatus() {
  char buf[448];
  IPAddress ip = ETH.localIP();
  if (!ip) {
    if (WiFi.getMode() & WIFI_AP) ip = WiFi.softAPIP();
    else ip = WiFi.localIP();
  }
  uint64_t media = 0;
  for (int i = 0; i < _volCount; i++) {
    if (_vols[i].store && _vols[i].store->mounted()) media += _vols[i].store->mediaBytes();
  }
  uint64_t media_mb = media / (1024ULL * 1024ULL);
  uint64_t used = _used;
  uint64_t total = _total ? _total : media;
  const char *st =
      _usageState == 2 ? "ready" : (_usageState == 1 ? "scanning" : (_usageState == 3 ? "fail" : "pending"));
  char name_esc[48];
  jsonEscapeTo(_name, name_esc, sizeof(name_esc));
  const char *lab = _multi ? "multi" : (_volCount ? _vols[0].name : "none");
  char label_esc[32];
  jsonEscapeTo(lab, label_esc, sizeof(label_esc));
  snprintf(buf, sizeof(buf),
           "{\"name\":\"%s\",\"storage\":\"%s\",\"multi\":%s,\"volumes\":%d,\"auth\":%s,"
           "\"ip\":\"%u.%u.%u.%u\",\"card_mb\":%llu,\"used\":%llu,\"total\":%llu,"
           "\"mounted\":%s,\"usage\":\"%s\",\"file_port\":%u}",
           name_esc, label_esc, _multi ? "true" : "false", _volCount,
           authEnabled() ? "true" : "false", ip[0], ip[1], ip[2], ip[3],
           (unsigned long long)media_mb, (unsigned long long)used, (unsigned long long)total,
           anyMounted() ? "true" : "false", st, (unsigned)_filePort);
  _ui->send(200, "application/json", buf);
}

void WebFileManager::handleList() {
  noteBusy();
  if (!anyMounted()) {
    sendJsonErr(*_ui, 503, "not mounted");
    return;
  }
  String path = _ui->hasArg("path") ? _ui->arg("path") : "/";
  if (!path.length()) path = "/";

  WfmStorage *store = nullptr;
  String local;
  bool volRoot = false;
  if (!resolve(path, store, local, &volRoot)) {
    sendJsonErr(*_ui, 400, "invalid path");
    return;
  }

  // Virtual root: list volume names as folders.
  if (_multi && path == "/") {
    char buf[512];
    int used = snprintf(buf, sizeof(buf), "{\"path\":\"/\",\"entries\":[");
    bool first = true;
    for (int i = 0; i < _volCount; i++) {
      if (!_vols[i].store || !_vols[i].store->mounted()) continue;
      char name_esc[40];
      jsonEscapeTo(_vols[i].name, name_esc, sizeof(name_esc));
      const char *vt = volumeTypeOf(_vols[i].name);
      used += snprintf(buf + used, sizeof(buf) - (size_t)used,
                       "%s{\"name\":\"%s\",\"dir\":true,\"volume\":true,\"vtype\":\"%s\",\"size\":0}",
                       first ? "" : ",", name_esc, vt);
      first = false;
    }
    used += snprintf(buf + used, sizeof(buf) - (size_t)used, "],\"truncated\":false,\"count\":%d}",
                     _volCount);
    _ui->send(200, "application/json", buf);
    return;
  }

  if (!store || !store->mounted()) {
    sendJsonErr(*_ui, 503, "volume not mounted");
    return;
  }

  store->lock();
  File dir = store->fs().open(local);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    store->unlock();
    sendJsonErr(*_ui, 404, "not a directory");
    return;
  }

  const size_t cap = 64 * 1024;
  char *buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) buf = (char *)malloc(cap);
  if (!buf) {
    dir.close();
    store->unlock();
    sendJsonErr(*_ui, 500, "oom");
    return;
  }

  char path_esc[200];
  jsonEscapeTo(path.c_str(), path_esc, sizeof(path_esc));
  int used = snprintf(buf, cap, "{\"path\":\"%s\",\"entries\":[", path_esc);
  if (used < 0) used = 0;

  bool first = true, truncated = false;
  int count = 0;
  const int kMax = 250;
  File f = dir.openNextFile();
  while (f) {
    if (count >= kMax || (size_t)used + 280 >= cap) {
      truncated = true;
      f.close();
      while (true) {
        File skip = dir.openNextFile();
        if (!skip) break;
        skip.close();
      }
      break;
    }
    const char *raw = f.name();
    const char *base = raw ? strrchr(raw, '/') : nullptr;
    base = base ? (base + 1) : (raw ? raw : "");
    bool is_dir = f.isDirectory();
    uint32_t sz = is_dir ? 0 : (uint32_t)f.size();
    f.close();
    if (!base[0] || (base[0] == '.' && (base[1] == 0 || (base[1] == '.' && base[2] == 0))) ||
        isRecWorkName(base) || isMuxTempName(base)) {
      f = dir.openNextFile();
      continue;
    }
    char name_esc[160];
    jsonEscapeTo(base, name_esc, sizeof(name_esc));
    int n = snprintf(buf + used, cap - (size_t)used, "%s{\"name\":\"%s\",\"dir\":%s,\"size\":%lu}",
                     first ? "" : ",", name_esc, is_dir ? "true" : "false", (unsigned long)sz);
    if (n < 0 || (size_t)n >= cap - (size_t)used) {
      truncated = true;
      break;
    }
    used += n;
    first = false;
    count++;
    f = dir.openNextFile();
  }
  dir.close();
  store->unlock();

  int n = snprintf(buf + used, cap - (size_t)used, "],\"truncated\":%s,\"count\":%d}",
                   truncated ? "true" : "false", count);
  if (n > 0) used += n;
  _ui->send(200, "application/json", buf);
  free(buf);
}

void WebFileManager::handleText() {
  noteBusy();
  String path = _ui->hasArg("path") ? _ui->arg("path") : "";
  WfmStorage *store = nullptr;
  String local;
  if (!resolve(path, store, local) || !store || !isTextPath(local)) {
    sendJsonErr(*_ui, 400, "not a text path");
    return;
  }
  const size_t kMax = 48 * 1024;
  store->lock();
  File f = store->fs().open(local, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    store->unlock();
    sendJsonErr(*_ui, 404, "not found");
    return;
  }
  size_t fsz = f.size();
  size_t n = fsz > kMax ? kMax : fsz;
  char *raw = (char *)malloc(n + 1);
  if (!raw) {
    f.close();
    store->unlock();
    sendJsonErr(*_ui, 500, "oom");
    return;
  }
  size_t got = f.readBytes(raw, n);
  f.close();
  store->unlock();
  raw[got] = 0;

  size_t jcap = got * 2 + 128;
  char *json = (char *)malloc(jcap);
  if (!json) {
    free(raw);
    sendJsonErr(*_ui, 500, "oom");
    return;
  }
  size_t o = 0;
  o += snprintf(json + o, jcap - o, "{\"truncated\":%s,\"size\":%u,\"text\":\"",
                (fsz > kMax) ? "true" : "false", (unsigned)fsz);
  for (size_t i = 0; i < got && o + 8 < jcap; i++) {
    unsigned char c = (unsigned char)raw[i];
    if (c == '"' || c == '\\') {
      json[o++] = '\\';
      json[o++] = (char)c;
    } else if (c == '\n') {
      json[o++] = '\\';
      json[o++] = 'n';
    } else if (c == '\r') {
      json[o++] = '\\';
      json[o++] = 'r';
    } else if (c == '\t') {
      json[o++] = '\\';
      json[o++] = 't';
    } else if (c < 0x20) {
      o += snprintf(json + o, jcap - o, "\\u%04x", c);
    } else {
      json[o++] = (char)c;
    }
  }
  if (o + 3 < jcap) {
    json[o++] = '"';
    json[o++] = '}';
    json[o] = 0;
  }
  free(raw);
  _ui->send(200, "application/json", json);
  free(json);
}

void WebFileManager::handleTextSave() {
  String path = _ui->hasArg("path") ? _ui->arg("path") : "";
  WfmStorage *store = nullptr;
  String local;
  if (!resolve(path, store, local) || !store || !isTextPath(local)) {
    sendJsonErr(*_ui, 400, "not a text path");
    return;
  }

  // Prefer raw POST body ("plain"); also accept form field "text".
  // Empty body is valid (clear file).
  String body;
  if (_ui->hasArg("plain")) {
    body = _ui->arg("plain");
  } else if (_ui->hasArg("text")) {
    body = _ui->arg("text");
  } else {
    sendJsonErr(*_ui, 400, "missing text body");
    return;
  }

  const size_t kMax = 48 * 1024;
  if (body.length() > kMax) {
    sendJsonErr(*_ui, 413, "text too large (max 48KB)");
    return;
  }

  store->lock();
  // Overwrite atomically-ish: truncate via FILE_WRITE.
  File f = store->fs().open(local, FILE_WRITE);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    store->unlock();
    sendJsonErr(*_ui, 404, "not found");
    return;
  }
  size_t wrote = 0;
  if (body.length()) {
    wrote = f.write((const uint8_t *)body.c_str(), body.length());
  }
  f.flush();
  f.close();
  store->unlock();

  if (wrote != body.length()) {
    sendJsonErr(*_ui, 500, "write incomplete");
    return;
  }

  char buf[96];
  snprintf(buf, sizeof(buf), "{\"ok\":1,\"path\":\"");
  // Minimal JSON — path may need escaping; keep simple for ASCII paths.
  String out = "{\"ok\":1,\"bytes\":";
  out += String((unsigned)wrote);
  out += ",\"path\":\"";
  for (size_t i = 0; i < path.length(); i++) {
    char c = path[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += "\"}";
  _ui->send(200, "application/json", out);
  (void)buf;
}

void WebFileManager::handleDetails() {
  noteBusy();
  String path = _ui->hasArg("path") ? _ui->arg("path") : "";
  WfmStorage *store = nullptr;
  String local;
  if (!resolve(path, store, local) || !store) {
    sendJsonErr(*_ui, 400, "invalid path");
    return;
  }
  store->lock();
  File f = store->fs().open(local);
  if (!f) {
    store->unlock();
    sendJsonErr(*_ui, 404, "not found");
    return;
  }
  bool is_dir = f.isDirectory();
  size_t sz = is_dir ? 0 : f.size();
  f.close();
  store->unlock();
  char path_esc[200], name_esc[120];
  jsonEscapeTo(path.c_str(), path_esc, sizeof(path_esc));
  jsonEscapeTo(baseName(path).c_str(), name_esc, sizeof(name_esc));
  char buf[320];
  snprintf(buf, sizeof(buf), "{\"path\":\"%s\",\"name\":\"%s\",\"dir\":%s,\"size\":%lu}", path_esc,
           name_esc, is_dir ? "true" : "false", (unsigned long)sz);
  _ui->send(200, "application/json", buf);
}

bool WebFileManager::deleteRecursive(WfmStorage &store, const String &path) {
  store.lock();
  File f = store.fs().open(path);
  if (!f) {
    store.unlock();
    return false;
  }
  if (!f.isDirectory()) {
    f.close();
    bool ok = store.fs().remove(path);
    store.unlock();
    return ok;
  }
  File child = f.openNextFile();
  while (child) {
    String full = child.name();
    if (full[0] != '/') full = joinPath(path, baseName(full));
    child.close();
    store.unlock();
    if (!deleteRecursive(store, full)) return false;
    store.lock();
    child = f.openNextFile();
  }
  f.close();
  bool ok = (path == "/") ? true : store.fs().rmdir(path);
  store.unlock();
  return ok;
}

void WebFileManager::handleDelete() {
  noteBusy();
  String path = _ui->hasArg("path") ? _ui->arg("path") : "";
  WfmStorage *store = nullptr;
  String local;
  bool atVolRoot = false;
  if (!resolve(path, store, local, &atVolRoot) || !store || local == "/" || path == "/" ||
      atVolRoot) {
    sendJsonErr(*_ui, 400, "invalid path");
    return;
  }
  if (!deleteRecursive(*store, local)) {
    sendJsonErr(*_ui, 500, "delete failed");
    return;
  }
  sendJsonOk(*_ui);
}

void WebFileManager::handleMkdir() {
  String dir = _ui->hasArg("dir") ? _ui->arg("dir") : (_ui->hasArg("path") ? _ui->arg("path") : "/");
  String name = _ui->hasArg("name") ? sanitizeUploadName(_ui->arg("name")) : "";
  if (!name.length() || name == "upload.bin") {
    sendJsonErr(*_ui, 400, "invalid name");
    return;
  }
  WfmStorage *store = nullptr;
  String localDir;
  if (!resolve(dir, store, localDir) || !store) {
    sendJsonErr(*_ui, 400, "invalid folder");
    return;
  }
  if (_multi && dir == "/") {
    sendJsonErr(*_ui, 400, "open a volume first");
    return;
  }
  String full = joinPath(localDir, name);
  store->lock();
  bool exists = store->fs().exists(full);
  store->unlock();
  if (exists) {
    sendJsonErr(*_ui, 409, "already exists");
    return;
  }
  if (!store->mkdir(full.c_str())) {
    sendJsonErr(*_ui, 500, "mkdir failed");
    return;
  }
  sendJsonOk(*_ui);
}

void WebFileManager::handleCreateFile() {
  String dir = _ui->hasArg("dir") ? _ui->arg("dir") : "/";
  String name = _ui->hasArg("name") ? sanitizeUploadName(_ui->arg("name")) : "";
  if (!name.length() || name == "upload.bin") {
    sendJsonErr(*_ui, 400, "invalid name");
    return;
  }
  WfmStorage *store = nullptr;
  String localDir;
  if (!resolve(dir, store, localDir) || !store) {
    sendJsonErr(*_ui, 400, "invalid folder");
    return;
  }
  if (_multi && dir == "/") {
    sendJsonErr(*_ui, 400, "open a volume first");
    return;
  }
  String full = joinPath(localDir, name);
  store->lock();
  if (store->fs().exists(full)) {
    store->unlock();
    sendJsonErr(*_ui, 409, "already exists");
    return;
  }
  File f = store->fs().open(full, FILE_WRITE);
  bool ok = (bool)f;
  if (f) {
    f.close();
  }
  store->unlock();
  if (!ok) {
    sendJsonErr(*_ui, 500, "create failed");
    return;
  }
  sendJsonOk(*_ui);
}

void WebFileManager::handleRename() {
  String path = _ui->hasArg("path") ? _ui->arg("path") : "";
  String name = sanitizeUploadName(_ui->hasArg("name") ? _ui->arg("name") : "");
  WfmStorage *store = nullptr;
  String local;
  if (!resolve(path, store, local) || !store || local == "/" || !name.length()) {
    sendJsonErr(*_ui, 400, "invalid rename");
    return;
  }
  String dest = joinPath(parentDir(local), name);
  if (!pathOk(dest) || store->exists(dest.c_str())) {
    sendJsonErr(*_ui, 400, "target exists or invalid");
    return;
  }
  if (!store->rename(local.c_str(), dest.c_str())) {
    sendJsonErr(*_ui, 500, "rename failed");
    return;
  }
  sendJsonOk(*_ui);
}

bool WebFileManager::copyFile(WfmStorage &fromStore, const String &from, WfmStorage &toStore,
                              const String &to) {
  fromStore.lock();
  File src = fromStore.fs().open(from, FILE_READ);
  if (!src || src.isDirectory()) {
    if (src) src.close();
    fromStore.unlock();
    return false;
  }
  // Hold both locks carefully: unlock from then lock to if different.
  uint8_t buf[2048];
  bool ok = true;
  if (&fromStore == &toStore) {
    File dst = fromStore.fs().open(to, FILE_WRITE);
    if (!dst) {
      src.close();
      fromStore.unlock();
      return false;
    }
    while (src.available()) {
      size_t n = src.read(buf, sizeof(buf));
      if (!n) break;
      if (dst.write(buf, n) != n) {
        ok = false;
        break;
      }
    }
    src.close();
    dst.close();
    fromStore.unlock();
    return ok;
  }
  // Cross-volume: read all chunks releasing between stores.
  fromStore.unlock();
  toStore.lock();
  File dst = toStore.fs().open(to, FILE_WRITE);
  toStore.unlock();
  if (!dst) {
    fromStore.lock();
    src = fromStore.fs().open(from, FILE_READ);
    if (src) src.close();
    fromStore.unlock();
    return false;
  }
  fromStore.lock();
  src = fromStore.fs().open(from, FILE_READ);
  if (!src) {
    fromStore.unlock();
    toStore.lock();
    dst.close();
    toStore.unlock();
    return false;
  }
  while (src.available()) {
    size_t n = src.read(buf, sizeof(buf));
    if (!n) break;
    fromStore.unlock();
    toStore.lock();
    size_t w = dst.write(buf, n);
    toStore.unlock();
    fromStore.lock();
    if (w != n) {
      ok = false;
      break;
    }
  }
  src.close();
  fromStore.unlock();
  toStore.lock();
  dst.close();
  toStore.unlock();
  return ok;
}

void WebFileManager::handleCopy() {
  noteBusy();
  String from = _ui->hasArg("from") ? _ui->arg("from") : "";
  String to = _ui->hasArg("to") ? _ui->arg("to") : "";
  WfmStorage *fs = nullptr, *ts = nullptr;
  String fl, tl;
  if (!resolve(from, fs, fl) || !resolve(to, ts, tl) || !fs || !ts || from == to) {
    sendJsonErr(*_ui, 400, "invalid copy");
    return;
  }
  fs->lock();
  File f = fs->fs().open(fl);
  if (!f) {
    fs->unlock();
    sendJsonErr(*_ui, 404, "source missing");
    return;
  }
  bool is_dir = f.isDirectory();
  f.close();
  fs->unlock();
  if (is_dir) {
    sendJsonErr(*_ui, 400, "folder copy not supported");
    return;
  }
  if (!copyFile(*fs, fl, *ts, tl)) {
    sendJsonErr(*_ui, 500, "copy failed");
    return;
  }
  sendJsonOk(*_ui);
}

void WebFileManager::handleMove() {
  noteBusy();
  String from = _ui->hasArg("from") ? _ui->arg("from") : "";
  String to = _ui->hasArg("to") ? _ui->arg("to") : "";
  WfmStorage *fs = nullptr, *ts = nullptr;
  String fl, tl;
  if (!resolve(from, fs, fl) || !resolve(to, ts, tl) || !fs || !ts || from == to || fl == "/") {
    sendJsonErr(*_ui, 400, "invalid move");
    return;
  }
  if (fs == ts) {
    if (ts->exists(tl.c_str())) {
      sendJsonErr(*_ui, 400, "target exists");
      return;
    }
    if (!fs->rename(fl.c_str(), tl.c_str())) {
      sendJsonErr(*_ui, 500, "move failed");
      return;
    }
  } else {
    if (!copyFile(*fs, fl, *ts, tl) || !deleteRecursive(*fs, fl)) {
      sendJsonErr(*_ui, 500, "cross-volume move failed");
      return;
    }
  }
  sendJsonOk(*_ui);
}

void WebFileManager::searchWalk(SearchState &st, WfmStorage &store, const String &dir,
                                const String &pubPrefix, int depth) {
  if (st.truncated || depth > 8) return;
  store.lock();
  File d = store.fs().open(dir);
  if (!d || !d.isDirectory()) {
    if (d) d.close();
    store.unlock();
    return;
  }
  File f = d.openNextFile();
  while (f && !st.truncated) {
    const char *raw = f.name();
    const char *base = raw ? strrchr(raw, '/') : nullptr;
    base = base ? (base + 1) : (raw ? raw : "");
    bool is_dir = f.isDirectory();
    uint32_t sz = is_dir ? 0 : (uint32_t)f.size();
    f.close();
    if (base[0] && !(base[0] == '.' && (base[1] == 0 || (base[1] == '.' && base[2] == 0))) &&
        !isRecWorkName(base) && !isMuxTempName(base)) {
      String fullLocal = joinPath(dir, base);
      String fullPub =
          (pubPrefix == "/") ? joinPath(pubPrefix, base) : (pubPrefix + "/" + String(base));
      if (pubPrefix != "/" && dir != "/") {
        // pubPrefix is /Vol ; local /a/b -> /Vol/a/b
        fullPub = pubPrefix + fullLocal;
      } else if (pubPrefix != "/" && dir == "/") {
        fullPub = pubPrefix + "/" + String(base);
      } else {
        fullPub = fullLocal;
      }
      String b = base;
      b.toLowerCase();
      if (b.indexOf(st.q) >= 0) {
        if (st.count >= 80 || (size_t)st.used + 300 >= st.cap) {
          st.truncated = true;
        } else {
          char name_esc[120], path_esc[200];
          jsonEscapeTo(base, name_esc, sizeof(name_esc));
          jsonEscapeTo(fullPub.c_str(), path_esc, sizeof(path_esc));
          int n = snprintf(st.buf + st.used, st.cap - (size_t)st.used,
                           "%s{\"name\":\"%s\",\"path\":\"%s\",\"dir\":%s,\"size\":%lu}",
                           st.first ? "" : ",", name_esc, path_esc, is_dir ? "true" : "false",
                           (unsigned long)sz);
          if (n > 0) {
            st.used += n;
            st.first = false;
            st.count++;
          }
        }
      }
      if (is_dir && !st.truncated) {
        store.unlock();
        searchWalk(st, store, fullLocal, pubPrefix, depth + 1);
        store.lock();
      }
    }
    f = d.openNextFile();
  }
  d.close();
  store.unlock();
}

void WebFileManager::handleSearch() {
  noteBusy();
  String q = _ui->hasArg("q") ? _ui->arg("q") : "";
  q.trim();
  q.toLowerCase();
  if (!q.length()) {
    sendJsonErr(*_ui, 400, "invalid search");
    return;
  }
  const size_t cap = 48 * 1024;
  char *buf = (char *)malloc(cap);
  if (!buf) {
    sendJsonErr(*_ui, 500, "oom");
    return;
  }
  char q_esc[64];
  jsonEscapeTo(q.c_str(), q_esc, sizeof(q_esc));
  SearchState st{};
  st.buf = buf;
  st.cap = cap;
  st.used = snprintf(buf, cap, "{\"q\":\"%s\",\"results\":[", q_esc);
  if (st.used < 0) st.used = 0;
  st.count = 0;
  st.truncated = false;
  st.first = true;
  st.q = q.c_str();
  for (int i = 0; i < _volCount && !st.truncated; i++) {
    if (!_vols[i].store || !_vols[i].store->mounted()) continue;
    String prefix = _multi ? (String("/") + _vols[i].name) : String("/");
    searchWalk(st, *_vols[i].store, "/", prefix, 0);
  }
  int n = snprintf(buf + st.used, cap - (size_t)st.used, "],\"truncated\":%s,\"count\":%d}",
                   st.truncated ? "true" : "false", st.count);
  if (n > 0) st.used += n;
  _ui->send(200, "application/json", buf);
  free(buf);
}

bool WebFileManager::parseByteRange(const String &rangeHdr, size_t fileSize, size_t &start,
                                    size_t &end) {
  if (!rangeHdr.startsWith("bytes=") || fileSize == 0) return false;
  String spec = rangeHdr.substring(6);
  int comma = spec.indexOf(',');
  if (comma >= 0) spec = spec.substring(0, comma);
  spec.trim();
  int dash = spec.indexOf('-');
  if (dash < 0) return false;
  String a = spec.substring(0, dash);
  String b = spec.substring(dash + 1);
  a.trim();
  b.trim();
  if (!a.length() && !b.length()) return false;
  if (!a.length()) {
    size_t n = (size_t)strtoul(b.c_str(), nullptr, 10);
    if (!n) return false;
    if (n > fileSize) n = fileSize;
    start = fileSize - n;
    end = fileSize - 1;
    return true;
  }
  start = (size_t)strtoul(a.c_str(), nullptr, 10);
  if (start >= fileSize) return false;
  end = b.length() ? (size_t)strtoul(b.c_str(), nullptr, 10) : (fileSize - 1);
  if (end >= fileSize) end = fileSize - 1;
  return end >= start;
}

void WebFileManager::streamFile(WfmStorage &store, WebServer &srv, File &f, size_t start,
                                size_t length) {
  noteBusy();
  NetworkClient client = srv.client();
  client.setNoDelay(true);
  client.setTimeout(120000);
  size_t bufSz = 0;
  uint8_t *buf = allocXferBuf(128 * 1024, &bufSz);
  uint8_t stackBuf[4096];
  bool heap = buf != nullptr;
  if (!heap) {
    buf = stackBuf;
    bufSz = sizeof(stackBuf);
  }
  store.lock();
  if (start) f.seek(start);
  store.unlock();
  size_t remaining = length;
  while (remaining && client.connected()) {
    size_t chunk = remaining < bufSz ? remaining : bufSz;
    store.lock();
    size_t n = f.read(buf, chunk);
    store.unlock();
    if (!n) break;
    size_t off = 0;
    while (off < n && client.connected()) {
      size_t w = client.write(buf + off, n - off);
      if (!w) {
        delay(1);
        w = client.write(buf + off, n - off);
        if (!w) break;
      }
      off += w;
    }
    if (off < n) break;
    remaining -= n;
    noteBusy();
    vTaskDelay(1);
  }
  if (heap) free(buf);
}

void WebFileManager::sendFileWithRanges(WfmStorage &store, WebServer &srv, File &f,
                                        const String &path, bool attachment) {
  size_t fsz = f.size();
  String name = baseName(path);
  String disposition =
      String(attachment ? "attachment" : "inline") + "; filename=\"" + name + "\"";
  srv.sendHeader("Content-Disposition", disposition);
  srv.sendHeader("Cache-Control", "no-store");
  srv.sendHeader("Accept-Ranges", "bytes");
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.sendHeader("Access-Control-Expose-Headers", "Content-Range, Accept-Ranges, Content-Length");
  srv.sendHeader("Connection", "close");

  String range = srv.header("Range");
  size_t start = 0, end = fsz ? fsz - 1 : 0;
  bool partial = range.length() && fsz && parseByteRange(range, fsz, start, end);
  size_t length = fsz ? (end - start + 1) : 0;
  if (partial) {
    char cr[80];
    snprintf(cr, sizeof(cr), "bytes %u-%u/%u", (unsigned)start, (unsigned)end, (unsigned)fsz);
    srv.sendHeader("Content-Range", cr);
    srv.setContentLength(length);
    srv.send(206, mimeFor(path), "");
  } else {
    srv.setContentLength(fsz);
    srv.send(200, mimeFor(path), "");
    start = 0;
    length = fsz;
  }
  if (length) streamFile(store, srv, f, start, length);
}

void WebFileManager::handleView(WebServer &srv) {
  noteBusy();
  String path = srv.hasArg("path") ? srv.arg("path") : "";
  WfmStorage *store = nullptr;
  String local;
  if (!resolve(path, store, local) || !store) {
    srv.send(400, "text/plain", "invalid path");
    return;
  }
  if (isBlockedPreview(local)) {
    srv.send(415, "application/json",
             "{\"error\":\"format not previewable - use Download\"}");
    return;
  }
  store->lock();
  File f = store->fs().open(local, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    store->unlock();
    srv.send(404, "text/plain", "not found");
    return;
  }
  store->unlock();
  sendFileWithRanges(*store, srv, f, local, false);
  store->lock();
  f.close();
  store->unlock();
}

void WebFileManager::handleDownload(WebServer &srv) {
  noteBusy();
  String path = srv.hasArg("path") ? srv.arg("path") : "";
  WfmStorage *store = nullptr;
  String local;
  if (!resolve(path, store, local) || !store) {
    srv.send(400, "text/plain", "invalid path");
    return;
  }
  store->lock();
  File f = store->fs().open(local, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    store->unlock();
    srv.send(404, "text/plain", "not found");
    return;
  }
  store->unlock();
  sendFileWithRanges(*store, srv, f, local, true);
  store->lock();
  f.close();
  store->unlock();
}

void WebFileManager::handleUploadOptions(WebServer &srv) {
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  srv.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  srv.sendHeader("Access-Control-Max-Age", "86400");
  srv.send(204);
}

void WebFileManager::handleUploadStream(WebServer &srv) {
  noteBusy();
  HTTPUpload &up = srv.upload();
  if (up.status == UPLOAD_FILE_START) {
    _uploadOk = false;
    _uploadStore = nullptr;
    if (authEnabled() && !srv.authenticate(_authUser, _authPass)) {
      return;
    }
    if (_upload) {
      _upload.close();
    }
    String dir = srv.hasArg("dir") ? srv.arg("dir") : "/";
    WfmStorage *store = nullptr;
    String localDir;
    if (!resolve(dir, store, localDir) || !store) {
      _uploadOk = false;
      return;
    }
    if (_multi && dir == "/") {
      _uploadOk = false;  // must upload into a volume
      return;
    }
    String fname = sanitizeUploadName(up.filename);
    _uploadPath = (localDir == "/") ? ("/" + fname) : (localDir + "/" + fname);
    _uploadStore = store;
    store->lock();
    if (store->fs().exists(_uploadPath)) store->fs().remove(_uploadPath);
    _upload = store->fs().open(_uploadPath, FILE_WRITE);
    store->unlock();
    _uploadOk = (bool)_upload;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (_upload && _uploadStore && up.currentSize) {
      _uploadStore->lock();
      size_t w = _upload.write(up.buf, up.currentSize);
      _uploadStore->unlock();
      if (w != up.currentSize) _uploadOk = false;
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (_upload && _uploadStore) {
      _uploadStore->lock();
      _upload.flush();
      _upload.close();
      _uploadStore->unlock();
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (_upload && _uploadStore) {
      _uploadStore->lock();
      _upload.close();
      _uploadStore->fs().remove(_uploadPath);
      _uploadStore->unlock();
    }
    _uploadOk = false;
  }
}

void WebFileManager::handleUploadDone(WebServer &srv) {
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  if (_uploadOk && _uploadPath.length() && _uploadStore) {
    String pub = _uploadPath;
    if (_multi) {
      for (int i = 0; i < _volCount; i++) {
        if (_vols[i].store == _uploadStore) {
          pub = makePublicPath(_vols[i], _uploadPath);
          break;
        }
      }
    }
    char path_esc[200];
    jsonEscapeTo(pub.c_str(), path_esc, sizeof(path_esc));
    char buf[240];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"path\":\"%s\"}", path_esc);
    srv.send(200, "application/json", buf);
  } else {
    srv.send(500, "application/json",
             "{\"error\":\"upload failed (open a volume folder first if multi-storage)\"}");
  }
  _uploadPath = "";
  _uploadStore = nullptr;
  _uploadOk = false;
}
