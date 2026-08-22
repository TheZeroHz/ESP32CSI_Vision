#pragma once
/**
 * @file WebFileManager.h
 * @brief Explorer-style web UI + REST API over WiFi or Ethernet.
 *
 * Single or multiple volumes (SD + FFat/LittleFS/SPIFFS):
 * @code
 *   WfmStorageFS sd(...);
 *   WfmStorageLittleFS flash;
 *   WebFileManager wfm(sd);          // first volume
 *   wfm.addVolume("LittleFS", flash); // optional extras
 *   // Multi-volume paths look like /SD/file.jpg and /LittleFS/cfg.txt
 *   // Single-volume paths stay /file.jpg (no prefix).
 * @endcode
 */
#include <Arduino.h>
#include <WebServer.h>
#include <NetworkClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "WfmStorage.h"
#include "WfmNetwork.h"

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#ifndef WFM_MAX_VOLUMES
#define WFM_MAX_VOLUMES 6
#endif

struct WfmVolume {
  const char *name;
  WfmStorage *store;
};

class WebFileManager {
 public:
  explicit WebFileManager(WfmStorage &primary);

  /** Add another mount (SD + FFat, etc.). Call before begin(). */
  WebFileManager &addVolume(const char *name, WfmStorage &storage);

  WebFileManager &setPorts(uint16_t uiPort = 80, uint16_t filePort = 81);
  WebFileManager &setName(const char *name);

  /**
   * Show a "Camera" link back to another HTTP UI (e.g. EthH264Record on :80).
   * Pass 0 to hide. Call before begin().
   */
  WebFileManager &setHomePort(uint16_t port);
  uint16_t homePort() const { return _homePort; }

  /**
   * Optional HTTP Basic auth. Empty user disables auth.
   * Example: wfm.setAuth("admin", "secret");
   */
  WebFileManager &setAuth(const char *username, const char *password);

  bool begin();
  void loop();
  bool startFileTask(UBaseType_t priority = 2, BaseType_t core = 0,
                     uint32_t stackWords = 8192);

  /** Pause UI + file HTTP (e.g. while camera is recording to the same SD). */
  void setPaused(bool pause) { _paused = pause; }
  bool paused() const { return _paused; }
  /** True shortly after a directory/list/preview/transfer hit the SD card. */
  bool isBusy(uint32_t hold_ms = 1500) const {
    uint32_t t = _busy_ms;
    return t != 0 && (millis() - t) < hold_ms;
  }

  bool started() const { return _started; }
  bool authEnabled() const { return _authUser && _authUser[0]; }
  uint16_t uiPort() const { return _uiPort; }
  uint16_t filePort() const { return _filePort; }
  int volumeCount() const { return _volCount; }
  const WfmVolume *volume(int i) const {
    return (i >= 0 && i < _volCount) ? &_vols[i] : nullptr;
  }

  void refreshUsageAsync();

 private:
  struct SearchState {
    char *buf;
    size_t cap;
    int used;
    int count;
    bool truncated;
    bool first;
    const char *q;
  };

  WfmVolume _vols[WFM_MAX_VOLUMES]{};
  int _volCount = 0;
  bool _multi = false;

  WebServer *_ui = nullptr;
  WebServer *_file = nullptr;
  uint16_t _uiPort = 80;
  uint16_t _filePort = 81;
  uint16_t _homePort = 0;
  const char *_name = "WebFileManager";
  const char *_authUser = nullptr;
  const char *_authPass = "";
  bool _started = false;
  volatile bool _paused = false;
  volatile uint32_t _busy_ms = 0;
  TaskHandle_t _fileTask = nullptr;

  File _upload;
  String _uploadPath;
  WfmStorage *_uploadStore = nullptr;
  bool _uploadOk = false;

  volatile uint64_t _used = 0;
  volatile uint64_t _total = 0;
  volatile uint8_t _usageState = 0;

  static void fileTaskThunk(void *arg);
  static void usageTaskThunk(void *arg);

  void bindRoutes();
  void noteBusy() { _busy_ms = millis(); }
  bool requireAuth(WebServer &srv);
  void sendJsonOk(WebServer &srv);
  void sendJsonErr(WebServer &srv, int code, const char *msg);

  /** Map URL path -> storage + local FS path. */
  bool resolve(const String &path, WfmStorage *&store, String &local,
               bool *atVolumeRoot = nullptr) const;
  bool anyMounted() const;
  String makePublicPath(const WfmVolume &vol, const String &local) const;
  static const char *volumeTypeOf(const char *name);

  void handleRoot();
  void handlePing();
  void handleStatus();
  void handleVolumes();
  void handleList();
  void handleText();
  void handleTextSave();
  void handleDetails();
  void handleDelete();
  void handleMkdir();
  void handleCreateFile();
  void handleRename();
  void handleCopy();
  void handleMove();
  void handleSearch();
  void handleView(WebServer &srv);
  void handleDownload(WebServer &srv);
  void handleUploadStream(WebServer &srv);
  void handleUploadDone(WebServer &srv);
  void handleUploadOptions(WebServer &srv);

  void searchWalk(SearchState &st, WfmStorage &store, const String &dir, const String &pubPrefix,
                  int depth);
  bool copyFile(WfmStorage &fromStore, const String &from, WfmStorage &toStore, const String &to);
  bool deleteRecursive(WfmStorage &store, const String &path);
  bool parseByteRange(const String &rangeHdr, size_t fileSize, size_t &start, size_t &end);
  void streamFile(WfmStorage &store, WebServer &srv, File &f, size_t start, size_t length);
  void sendFileWithRanges(WfmStorage &store, WebServer &srv, File &f, const String &path,
                          bool attachment);
};
