#pragma once
/**
 * @file WfmUtil.h
 * @brief Shared helpers for WebFileManager (paths, JSON, MIME). Header-only / DRY.
 */
#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>

namespace wfm {

inline bool pathOk(const String &p) {
  if (!p.length() || p[0] != '/') return false;
  if (p.indexOf("..") >= 0) return false;
  if (p.length() > 200) return false;
  return true;
}

inline String joinPath(const String &dir, const String &name) {
  if (dir == "/") return String("/") + name;
  return dir + "/" + name;
}

inline String parentDir(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash <= 0) return "/";
  return path.substring(0, slash);
}

inline String baseName(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash < 0) return path;
  return path.substring(slash + 1);
}

inline void jsonEscapeTo(const char *s, char *out, size_t out_cap) {
  size_t o = 0;
  if (!s || out_cap < 2) {
    if (out_cap) out[0] = 0;
    return;
  }
  for (size_t i = 0; s[i] && o + 2 < out_cap; i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      if (o + 3 >= out_cap) break;
      out[o++] = '\\';
      out[o++] = c;
    } else if ((uint8_t)c >= 0x20) {
      out[o++] = c;
    }
  }
  out[o] = 0;
}

inline const char *mimeFor(const String &path) {
  String lower = path;
  lower.toLowerCase();
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
  if (lower.endsWith(".png")) return "image/png";
  if (lower.endsWith(".gif")) return "image/gif";
  if (lower.endsWith(".webp")) return "image/webp";
  if (lower.endsWith(".bmp")) return "image/bmp";
  if (lower.endsWith(".svg")) return "image/svg+xml";
  if (lower.endsWith(".mp4")) return "video/mp4";
  if (lower.endsWith(".webm")) return "video/webm";
  if (lower.endsWith(".avi")) return "video/x-msvideo";
  if (lower.endsWith(".mov")) return "video/quicktime";
  if (lower.endsWith(".mkv")) return "video/x-matroska";
  if (lower.endsWith(".mp3")) return "audio/mpeg";
  if (lower.endsWith(".wav")) return "audio/wav";
  if (lower.endsWith(".ogg") || lower.endsWith(".opus")) return "audio/ogg";
  if (lower.endsWith(".aac")) return "audio/aac";
  if (lower.endsWith(".m4a")) return "audio/mp4";
  if (lower.endsWith(".flac")) return "audio/flac";
  if (lower.endsWith(".html") || lower.endsWith(".htm")) return "text/html";
  if (lower.endsWith(".css")) return "text/css";
  if (lower.endsWith(".js")) return "application/javascript";
  if (lower.endsWith(".json")) return "application/json";
  if (lower.endsWith(".xml")) return "application/xml";
  if (lower.endsWith(".txt") || lower.endsWith(".log") || lower.endsWith(".md") ||
      lower.endsWith(".csv") || lower.endsWith(".ini") || lower.endsWith(".cfg") ||
      lower.endsWith(".nfo") || lower.endsWith(".srt"))
    return "text/plain; charset=utf-8";
  return "application/octet-stream";
}

inline bool isTextPath(const String &path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".txt") || lower.endsWith(".log") || lower.endsWith(".md") ||
         lower.endsWith(".csv") || lower.endsWith(".json") || lower.endsWith(".xml") ||
         lower.endsWith(".ini") || lower.endsWith(".cfg") || lower.endsWith(".nfo") ||
         lower.endsWith(".srt") || lower.endsWith(".html") || lower.endsWith(".htm") ||
         lower.endsWith(".css") || lower.endsWith(".js");
}

inline bool isBlockedPreview(const String &path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".avi") || lower.endsWith(".mkv") || lower.endsWith(".mov") ||
         lower.endsWith(".mjpg") || lower.endsWith(".mjpeg");
}

inline String sanitizeUploadName(const String &name) {
  String out;
  out.reserve(name.length());
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
        c == '>' || c == '|')
      continue;
    if ((uint8_t)c < 0x20) continue;
    out += c;
  }
  if (!out.length()) out = "upload.bin";
  if (out.length() > 64) out = out.substring(out.length() - 64);
  return out;
}

inline uint8_t *allocXferBuf(size_t prefer, size_t *outSz) {
  size_t want = prefer;
  uint8_t *p = (uint8_t *)heap_caps_malloc(want, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    want = 32 * 1024;
    p = (uint8_t *)heap_caps_malloc(want, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!p) {
    want = 16 * 1024;
    p = (uint8_t *)malloc(want);
  }
  if (outSz) *outSz = p ? want : 0;
  return p;
}

}  // namespace wfm
