#include "h264/ESP32P4_H264Mp4.h"

#include <Arduino.h>
#include <string.h>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio/ESP32P4_AacEnc.h"
#include "mem/ESP32P4_Psram.h"

struct NalRef {
  const uint8_t *data;
  uint32_t len;
  uint8_t type;
};

static void w32(std::vector<uint8_t> &b, uint32_t v) {
  b.push_back((uint8_t)(v >> 24));
  b.push_back((uint8_t)(v >> 16));
  b.push_back((uint8_t)(v >> 8));
  b.push_back((uint8_t)v);
}

static void w16(std::vector<uint8_t> &b, uint16_t v) {
  b.push_back((uint8_t)(v >> 8));
  b.push_back((uint8_t)v);
}

static void wbytes(std::vector<uint8_t> &b, const uint8_t *p, size_t n) {
  b.insert(b.end(), p, p + n);
}

static void wfourcc(std::vector<uint8_t> &b, const char *fcc) {
  b.push_back((uint8_t)fcc[0]);
  b.push_back((uint8_t)fcc[1]);
  b.push_back((uint8_t)fcc[2]);
  b.push_back((uint8_t)fcc[3]);
}

static void box(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &payload) {
  w32(out, (uint32_t)(8 + payload.size()));
  wfourcc(out, type);
  wbytes(out, payload.data(), payload.size());
}

static bool parse_annexb(const uint8_t *buf, size_t len, std::vector<NalRef> &nals) {
  nals.clear();
  size_t i = 0;
  while (i + 3 < len) {
    size_t sc = 0;
    if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) sc = 3;
    else if (i + 4 < len && buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 0 && buf[i + 3] == 1)
      sc = 4;
    if (!sc) {
      i++;
      continue;
    }
    size_t start = i + sc;
    size_t j = start;
    while (j + 3 < len) {
      if (buf[j] == 0 && buf[j + 1] == 0 &&
          (buf[j + 2] == 1 || (j + 4 <= len && buf[j + 2] == 0 && buf[j + 3] == 1)))
        break;
      j++;
    }
    if (j > start) {
      NalRef n{};
      n.data = buf + start;
      n.len = (uint32_t)(j - start);
      n.type = (uint8_t)(n.data[0] & 0x1F);
      nals.push_back(n);
    }
    i = j;
  }
  return !nals.empty();
}

static std::vector<uint8_t> make_dinf() {
  std::vector<uint8_t> url_body;
  url_body.push_back(0);
  url_body.push_back(0);
  url_body.push_back(0);
  url_body.push_back(1);
  std::vector<uint8_t> url;
  box(url, "url ", url_body);
  std::vector<uint8_t> dref_body;
  dref_body.push_back(0);
  dref_body.push_back(0);
  dref_body.push_back(0);
  dref_body.push_back(0);
  w32(dref_body, 1);
  wbytes(dref_body, url.data(), url.size());
  std::vector<uint8_t> dref;
  box(dref, "dref", dref_body);
  std::vector<uint8_t> dinf;
  box(dinf, "dinf", dref);
  return dinf;
}

static std::vector<uint8_t> make_hdlr(const char *type, const char *name) {
  std::vector<uint8_t> body;
  body.push_back(0);
  body.push_back(0);
  body.push_back(0);
  body.push_back(0);
  w32(body, 0);
  wfourcc(body, type);
  w32(body, 0);
  w32(body, 0);
  w32(body, 0);
  wbytes(body, (const uint8_t *)name, strlen(name) + 1);
  std::vector<uint8_t> hdlr;
  box(hdlr, "hdlr", body);
  return hdlr;
}

static uint8_t aac_sfi(uint32_t rate) {
  static const uint32_t kRates[] = {96000, 88200, 64000, 48000, 44100, 32000,
                                    24000, 22050, 16000, 12000, 11025, 8000};
  for (uint8_t i = 0; i < 12; i++) {
    if (kRates[i] == rate) return i;
  }
  return 8;
}

static void wdesc(std::vector<uint8_t> &b, uint8_t tag, const std::vector<uint8_t> &payload) {
  b.push_back(tag);
  size_t n = payload.size();
  if (n < 128) {
    b.push_back((uint8_t)n);
  } else {
    b.push_back((uint8_t)(0x80 | ((n >> 21) & 0x7F)));
    b.push_back((uint8_t)(0x80 | ((n >> 14) & 0x7F)));
    b.push_back((uint8_t)(0x80 | ((n >> 7) & 0x7F)));
    b.push_back((uint8_t)(n & 0x7F));
  }
  wbytes(b, payload.data(), payload.size());
}

static std::vector<uint8_t> make_esds(uint32_t rate_hz, uint16_t channels, uint32_t bitrate) {
  uint8_t asc[2];
  uint16_t bits = (uint16_t)((2u << 11) | ((uint16_t)aac_sfi(rate_hz) << 7) | ((channels & 0xF) << 3));
  asc[0] = (uint8_t)(bits >> 8);
  asc[1] = (uint8_t)bits;

  std::vector<uint8_t> dsi_body(asc, asc + 2);
  std::vector<uint8_t> dsi;
  wdesc(dsi, 0x05, dsi_body);

  std::vector<uint8_t> dec;
  dec.push_back(0x40);
  dec.push_back(0x15);
  dec.push_back(0);
  dec.push_back(1);
  dec.push_back(0);
  w32(dec, bitrate);
  w32(dec, bitrate);
  wbytes(dec, dsi.data(), dsi.size());
  std::vector<uint8_t> dec_box;
  wdesc(dec_box, 0x04, dec);

  std::vector<uint8_t> sl{0x02};
  std::vector<uint8_t> sl_box;
  wdesc(sl_box, 0x06, sl);

  std::vector<uint8_t> es;
  w16(es, 0);
  es.push_back(0);
  wbytes(es, dec_box.data(), dec_box.size());
  wbytes(es, sl_box.data(), sl_box.size());
  std::vector<uint8_t> es_box;
  wdesc(es_box, 0x03, es);

  std::vector<uint8_t> esds_body;
  esds_body.insert(esds_body.end(), {0, 0, 0, 0});
  wbytes(esds_body, es_box.data(), es_box.size());
  std::vector<uint8_t> esds;
  box(esds, "esds", esds_body);
  return esds;
}

static File mp4_open_retry(fs::FS &fs, const char *path, const char *mode) {
  File f;
  for (int i = 0; i < 10; i++) {
    f = fs.open(path, mode);
    if (f) return f;
    vTaskDelay(pdMS_TO_TICKS(40));
  }
  return f;
}

static bool mp4_write_all(File &out, const uint8_t *p, size_t n) {
  while (n) {
    size_t chunk = n > 4096 ? 4096 : n;
    size_t w = out.write(p, chunk);
    if (!w) {
      vTaskDelay(pdMS_TO_TICKS(30));
      w = out.write(p, chunk);
    }
    if (!w) return false;
    p += w;
    n -= w;
    vTaskDelay(1);
  }
  return true;
}

bool esp32p4_h264_annexb_to_mp4(fs::FS &fs, const char *h264_path, const char *mp4_path, uint16_t width,
                                uint16_t height, uint32_t duration_ms) {
  return esp32p4_h264_annexb_to_mp4(fs, h264_path, mp4_path, width, height, duration_ms, nullptr, 0,
                                    1, nullptr, 0, nullptr);
}

static void mux_set_pct(volatile uint8_t *p, uint8_t v) {
  if (p) *p = v;
}

bool esp32p4_h264_annexb_to_mp4(fs::FS &fs, const char *h264_path, const char *mp4_path, uint16_t width,
                                uint16_t height, uint32_t duration_ms, const char *pcm_path,
                                uint32_t pcm_rate_hz, uint16_t pcm_channels, const uint8_t *pcm_ram,
                                size_t pcm_ram_sz, volatile uint8_t *progress) {
  mux_set_pct(progress, 3);
  if (!h264_path || !mp4_path) return false;
  if (duration_ms < 1) duration_ms = 1;
  if (pcm_channels == 0) pcm_channels = 1;
  if (pcm_channels > 2) pcm_channels = 2;

  File in = mp4_open_retry(fs, h264_path, FILE_READ);
  if (!in) {
    Serial.printf("MP4: open %s failed\n", h264_path);
    return false;
  }
  size_t sz = in.size();
  if (!sz || sz > 16 * 1024 * 1024) {
    in.close();
    Serial.println("MP4: bad h264 size");
    return false;
  }
  uint8_t *buf = (uint8_t *)esp32p4_psram_alloc(sz);
  if (!buf) {
    in.close();
    Serial.println("MP4: alloc failed");
    return false;
  }
  if (in.read(buf, sz) != (int)sz) {
    in.close();
    esp32p4_psram_free(buf);
    Serial.println("MP4: read failed");
    return false;
  }
  in.close();
  mux_set_pct(progress, 18);

  std::vector<NalRef> nals;
  if (!parse_annexb(buf, sz, nals)) {
    esp32p4_psram_free(buf);
    Serial.println("MP4: no NALs");
    return false;
  }

  const uint8_t *sps = nullptr;
  const uint8_t *pps = nullptr;
  uint32_t sps_len = 0, pps_len = 0;
  std::vector<NalRef> samples;
  samples.reserve(nals.size());

  for (const auto &n : nals) {
    if (n.type == 7) {
      sps = n.data;
      sps_len = n.len;
    } else if (n.type == 8) {
      pps = n.data;
      pps_len = n.len;
    } else if (n.type == 1 || n.type == 5) {
      samples.push_back(n);
    }
  }

  if (!sps || !pps || samples.empty()) {
    esp32p4_psram_free(buf);
    Serial.printf("MP4: need SPS/PPS/slices (sps=%u pps=%u samples=%u)\n", sps ? 1u : 0u, pps ? 1u : 0u,
                  (unsigned)samples.size());
    return false;
  }

  mux_set_pct(progress, 38);

  // Optional mic PCM → AAC-LC (browsers reject sowt/PCM in MP4).
  uint8_t *pcm = nullptr;
  size_t pcm_sz = 0;
  bool pcm_owned = false;
  if (pcm_ram && pcm_ram_sz >= 320 && pcm_rate_hz >= 8000) {
    pcm = (uint8_t *)pcm_ram;
    pcm_sz = pcm_ram_sz;
  } else if (pcm_path && pcm_path[0] && pcm_rate_hz >= 8000) {
    File af = mp4_open_retry(fs, pcm_path, FILE_READ);
    if (af) {
      pcm_sz = af.size();
      if (pcm_sz > 0 && pcm_sz <= 32 * 1024 * 1024) {
        pcm = (uint8_t *)esp32p4_psram_alloc(pcm_sz);
        if (pcm && af.read(pcm, pcm_sz) == (int)pcm_sz) {
          pcm_owned = true;
        } else {
          esp32p4_psram_free(pcm);
          pcm = nullptr;
          pcm_sz = 0;
          Serial.println("MP4: PCM read/alloc failed - video-only");
        }
      } else {
        Serial.println("MP4: bad PCM size - video-only");
        pcm_sz = 0;
      }
      af.close();
    } else {
      Serial.printf("MP4: open PCM %s failed - video-only\n", pcm_path);
    }
  }

  std::vector<uint8_t> aac;
  std::vector<uint32_t> aac_sizes;
  uint32_t aac_bitrate = 32000;
  mux_set_pct(progress, 48);
  if (pcm && pcm_sz >= 320) {
    if (pcm_rate_hz >= 44100) aac_bitrate = 64000;
    else if (pcm_rate_hz >= 22050) aac_bitrate = 48000;
    const size_t nsamp = pcm_sz / 2;
    if (!esp32p4_pcm16_to_aac_lc((const int16_t *)pcm, nsamp, pcm_rate_hz, pcm_channels, aac_bitrate,
                                 aac, aac_sizes)) {
      Serial.println("MP4: AAC encode failed - video-only");
      aac.clear();
      aac_sizes.clear();
    }
  }
  if (pcm && pcm_owned) {
    esp32p4_psram_free(pcm);
    pcm = nullptr;
  }
  mux_set_pct(progress, 72);

  std::vector<uint8_t> mdat_payload;
  mdat_payload.reserve(sz + aac.size());
  std::vector<uint32_t> sample_sizes;
  sample_sizes.reserve(samples.size());
  for (const auto &s : samples) {
    w32(mdat_payload, s.len);
    wbytes(mdat_payload, s.data, s.len);
    sample_sizes.push_back(4 + s.len);
  }
  const uint32_t video_mdat_bytes = (uint32_t)mdat_payload.size();
  if (!aac.empty()) {
    wbytes(mdat_payload, aac.data(), aac.size());
  }

  std::vector<uint8_t> avcC;
  avcC.push_back(1);
  avcC.push_back(sps[1]);
  avcC.push_back(sps[2]);
  avcC.push_back(sps[3]);
  avcC.push_back(0xFF);
  avcC.push_back(0xE1);
  w16(avcC, (uint16_t)sps_len);
  wbytes(avcC, sps, sps_len);
  avcC.push_back(1);
  w16(avcC, (uint16_t)pps_len);
  wbytes(avcC, pps, pps_len);

  const uint32_t movie_timescale = 1000;
  const uint32_t video_duration = duration_ms;
  const uint32_t aac_frames = (uint32_t)aac_sizes.size();
  const uint32_t audio_duration_units = aac_frames * 1024u;
  const uint32_t audio_duration_ms =
      (aac_frames && pcm_rate_hz)
          ? (uint32_t)((uint64_t)audio_duration_units * 1000ULL / (uint64_t)pcm_rate_hz)
          : 0;
  const uint32_t movie_duration =
      audio_duration_ms > video_duration ? audio_duration_ms : video_duration;
  const uint32_t sample_delta =
      samples.size() ? (uint32_t)((video_duration + samples.size() - 1) / samples.size()) : 1;
  const uint32_t matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  const bool has_audio = aac_frames > 0;

  std::vector<uint8_t> ftyp;
  {
    std::vector<uint8_t> body;
    wfourcc(body, "isom");
    w32(body, 0x200);
    wfourcc(body, "isom");
    wfourcc(body, "iso2");
    wfourcc(body, "avc1");
    wfourcc(body, "mp41");
    box(ftyp, "ftyp", body);
  }
  const uint32_t video_data_offset = (uint32_t)ftyp.size() + 8;
  const uint32_t audio_data_offset = video_data_offset + video_mdat_bytes;

  std::vector<uint8_t> dinf = make_dinf();

  // ---- video trak ----
  std::vector<uint8_t> v_mdhd;
  {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, 0);
    w32(body, 0);
    w32(body, movie_timescale);
    w32(body, video_duration);
    w16(body, 0x55C4);
    w16(body, 0);
    box(v_mdhd, "mdhd", body);
  }
  std::vector<uint8_t> v_hdlr = make_hdlr("vide", "VideoHandler");

  std::vector<uint8_t> vmhd;
  {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 1});
    w16(body, 0);
    w16(body, 0);
    w16(body, 0);
    w16(body, 0);
    box(vmhd, "vmhd", body);
  }

  std::vector<uint8_t> stsd;
  {
    std::vector<uint8_t> avcC_box;
    box(avcC_box, "avcC", avcC);
    std::vector<uint8_t> avc1_body;
    for (int i = 0; i < 6; i++) avc1_body.push_back(0);
    w16(avc1_body, 1);
    w16(avc1_body, 0);
    w16(avc1_body, 0);
    w32(avc1_body, 0);
    w32(avc1_body, 0);
    w32(avc1_body, 0);
    w16(avc1_body, width);
    w16(avc1_body, height);
    w32(avc1_body, 0x00480000);
    w32(avc1_body, 0x00480000);
    w32(avc1_body, 0);
    w16(avc1_body, 1);
    for (int i = 0; i < 32; i++) avc1_body.push_back(0);
    w16(avc1_body, 0x0018);
    w16(avc1_body, 0xFFFF);
    wbytes(avc1_body, avcC_box.data(), avcC_box.size());
    std::vector<uint8_t> avc1;
    box(avc1, "avc1", avc1_body);
    std::vector<uint8_t> stsd_body;
    stsd_body.insert(stsd_body.end(), {0, 0, 0, 0});
    w32(stsd_body, 1);
    wbytes(stsd_body, avc1.data(), avc1.size());
    box(stsd, "stsd", stsd_body);
  }

  auto make_stts = [](uint32_t count, uint32_t delta) {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, 1);
    w32(body, count);
    w32(body, delta ? delta : 1);
    std::vector<uint8_t> stts;
    box(stts, "stts", body);
    return stts;
  };
  std::vector<uint8_t> stts = make_stts((uint32_t)samples.size(), sample_delta);

  std::vector<uint8_t> stss;
  {
    std::vector<uint32_t> sync;
    for (size_t i = 0; i < samples.size(); i++) {
      if (samples[i].type == 5) sync.push_back((uint32_t)(i + 1));
    }
    if (sync.empty()) sync.push_back(1);
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, (uint32_t)sync.size());
    for (uint32_t s : sync) w32(body, s);
    box(stss, "stss", body);
  }

  auto make_stsc = [](uint32_t samples_per_chunk) {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, 1);
    w32(body, 1);
    w32(body, samples_per_chunk);
    w32(body, 1);
    std::vector<uint8_t> stsc;
    box(stsc, "stsc", body);
    return stsc;
  };
  std::vector<uint8_t> stsc = make_stsc((uint32_t)samples.size());

  std::vector<uint8_t> stsz;
  {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, 0);
    w32(body, (uint32_t)sample_sizes.size());
    for (uint32_t s : sample_sizes) w32(body, s);
    box(stsz, "stsz", body);
  }

  auto make_stco = [](uint32_t offset) {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, 1);
    w32(body, offset);
    std::vector<uint8_t> stco;
    box(stco, "stco", body);
    return stco;
  };
  std::vector<uint8_t> stco = make_stco(video_data_offset);

  std::vector<uint8_t> stbl_payload;
  wbytes(stbl_payload, stsd.data(), stsd.size());
  wbytes(stbl_payload, stts.data(), stts.size());
  wbytes(stbl_payload, stss.data(), stss.size());
  wbytes(stbl_payload, stsc.data(), stsc.size());
  wbytes(stbl_payload, stsz.data(), stsz.size());
  wbytes(stbl_payload, stco.data(), stco.size());
  std::vector<uint8_t> stbl;
  box(stbl, "stbl", stbl_payload);

  std::vector<uint8_t> minf_payload;
  wbytes(minf_payload, vmhd.data(), vmhd.size());
  wbytes(minf_payload, dinf.data(), dinf.size());
  wbytes(minf_payload, stbl.data(), stbl.size());
  std::vector<uint8_t> minf;
  box(minf, "minf", minf_payload);

  std::vector<uint8_t> mdia_payload;
  wbytes(mdia_payload, v_mdhd.data(), v_mdhd.size());
  wbytes(mdia_payload, v_hdlr.data(), v_hdlr.size());
  wbytes(mdia_payload, minf.data(), minf.size());
  std::vector<uint8_t> mdia;
  box(mdia, "mdia", mdia_payload);

  std::vector<uint8_t> tkhd;
  {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 3});
    w32(body, 0);
    w32(body, 0);
    w32(body, 1);
    w32(body, 0);
    w32(body, video_duration);
    w32(body, 0);
    w32(body, 0);
    w16(body, 0);
    w16(body, 0);
    w16(body, 0);
    w16(body, 0);
    for (uint32_t m : matrix) w32(body, m);
    w32(body, ((uint32_t)width) << 16);
    w32(body, ((uint32_t)height) << 16);
    box(tkhd, "tkhd", body);
  }

  std::vector<uint8_t> trak_payload;
  wbytes(trak_payload, tkhd.data(), tkhd.size());
  wbytes(trak_payload, mdia.data(), mdia.size());
  std::vector<uint8_t> trak;
  box(trak, "trak", trak_payload);

  // ---- optional audio trak (AAC-LC mp4a) ----
  std::vector<uint8_t> a_trak;
  if (has_audio) {
    std::vector<uint8_t> a_mdhd;
    {
      std::vector<uint8_t> body;
      body.insert(body.end(), {0, 0, 0, 0});
      w32(body, 0);
      w32(body, 0);
      w32(body, pcm_rate_hz);
      w32(body, audio_duration_units);
      w16(body, 0x55C4);
      w16(body, 0);
      box(a_mdhd, "mdhd", body);
    }
    std::vector<uint8_t> a_hdlr = make_hdlr("soun", "SoundHandler");

    std::vector<uint8_t> smhd;
    {
      std::vector<uint8_t> body;
      body.insert(body.end(), {0, 0, 0, 0});
      w16(body, 0);
      w16(body, 0);
      box(smhd, "smhd", body);
    }

    std::vector<uint8_t> a_stsd;
    {
      std::vector<uint8_t> esds = make_esds(pcm_rate_hz, pcm_channels, aac_bitrate);
      std::vector<uint8_t> mp4a_body;
      for (int i = 0; i < 6; i++) mp4a_body.push_back(0);
      w16(mp4a_body, 1);
      w16(mp4a_body, 0);
      w16(mp4a_body, 0);
      w32(mp4a_body, 0);
      w16(mp4a_body, pcm_channels);
      w16(mp4a_body, 16);
      w16(mp4a_body, 0);
      w16(mp4a_body, 0);
      w32(mp4a_body, pcm_rate_hz << 16);
      wbytes(mp4a_body, esds.data(), esds.size());
      std::vector<uint8_t> mp4a;
      box(mp4a, "mp4a", mp4a_body);
      std::vector<uint8_t> body;
      body.insert(body.end(), {0, 0, 0, 0});
      w32(body, 1);
      wbytes(body, mp4a.data(), mp4a.size());
      box(a_stsd, "stsd", body);
    }

    std::vector<uint8_t> a_stts = make_stts(aac_frames, 1024);
    std::vector<uint8_t> a_stsc = make_stsc(aac_frames);
    std::vector<uint8_t> a_stsz;
    {
      std::vector<uint8_t> body;
      body.insert(body.end(), {0, 0, 0, 0});
      w32(body, 0);
      w32(body, aac_frames);
      for (uint32_t s : aac_sizes) w32(body, s);
      box(a_stsz, "stsz", body);
    }
    std::vector<uint8_t> a_stco = make_stco(audio_data_offset);

    std::vector<uint8_t> a_stbl_payload;
    wbytes(a_stbl_payload, a_stsd.data(), a_stsd.size());
    wbytes(a_stbl_payload, a_stts.data(), a_stts.size());
    wbytes(a_stbl_payload, a_stsc.data(), a_stsc.size());
    wbytes(a_stbl_payload, a_stsz.data(), a_stsz.size());
    wbytes(a_stbl_payload, a_stco.data(), a_stco.size());
    std::vector<uint8_t> a_stbl;
    box(a_stbl, "stbl", a_stbl_payload);

    std::vector<uint8_t> a_minf_payload;
    wbytes(a_minf_payload, smhd.data(), smhd.size());
    wbytes(a_minf_payload, dinf.data(), dinf.size());
    wbytes(a_minf_payload, a_stbl.data(), a_stbl.size());
    std::vector<uint8_t> a_minf;
    box(a_minf, "minf", a_minf_payload);

    std::vector<uint8_t> a_mdia_payload;
    wbytes(a_mdia_payload, a_mdhd.data(), a_mdhd.size());
    wbytes(a_mdia_payload, a_hdlr.data(), a_hdlr.size());
    wbytes(a_mdia_payload, a_minf.data(), a_minf.size());
    std::vector<uint8_t> a_mdia;
    box(a_mdia, "mdia", a_mdia_payload);

    std::vector<uint8_t> a_tkhd;
    {
      std::vector<uint8_t> body;
      body.insert(body.end(), {0, 0, 0, 3});
      w32(body, 0);
      w32(body, 0);
      w32(body, 2);  // track id
      w32(body, 0);
      w32(body, audio_duration_ms ? audio_duration_ms : movie_duration);
      w32(body, 0);
      w32(body, 0);
      w16(body, 0);
      w16(body, 0);
      w16(body, 0x0100);  // volume
      w16(body, 0);
      for (uint32_t m : matrix) w32(body, m);
      w32(body, 0);
      w32(body, 0);
      box(a_tkhd, "tkhd", body);
    }

    std::vector<uint8_t> a_trak_payload;
    wbytes(a_trak_payload, a_tkhd.data(), a_tkhd.size());
    wbytes(a_trak_payload, a_mdia.data(), a_mdia.size());
    box(a_trak, "trak", a_trak_payload);
  }

  std::vector<uint8_t> mvhd;
  {
    std::vector<uint8_t> body;
    body.insert(body.end(), {0, 0, 0, 0});
    w32(body, 0);
    w32(body, 0);
    w32(body, movie_timescale);
    w32(body, movie_duration);
    w32(body, 0x00010000);
    w16(body, 0x0100);
    w16(body, 0);
    w32(body, 0);
    w32(body, 0);
    for (uint32_t m : matrix) w32(body, m);
    for (int i = 0; i < 6; i++) w32(body, 0);
    w32(body, has_audio ? 3u : 2u);
    box(mvhd, "mvhd", body);
  }

  std::vector<uint8_t> moov_payload;
  wbytes(moov_payload, mvhd.data(), mvhd.size());
  wbytes(moov_payload, trak.data(), trak.size());
  if (has_audio) wbytes(moov_payload, a_trak.data(), a_trak.size());
  std::vector<uint8_t> moov;
  box(moov, "moov", moov_payload);

  uint8_t mdat_hdr[8];
  const uint32_t mdat_box = (uint32_t)(8 + mdat_payload.size());
  mdat_hdr[0] = (uint8_t)(mdat_box >> 24);
  mdat_hdr[1] = (uint8_t)(mdat_box >> 16);
  mdat_hdr[2] = (uint8_t)(mdat_box >> 8);
  mdat_hdr[3] = (uint8_t)mdat_box;
  mdat_hdr[4] = 'm';
  mdat_hdr[5] = 'd';
  mdat_hdr[6] = 'a';
  mdat_hdr[7] = 't';
  const size_t expect = ftyp.size() + sizeof(mdat_hdr) + mdat_payload.size() + moov.size();
  mux_set_pct(progress, 88);

  char tmp_path[96];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", mp4_path);
  fs.remove(tmp_path);
  fs.remove(mp4_path);

  vTaskDelay(pdMS_TO_TICKS(40));
  File out = mp4_open_retry(fs, tmp_path, FILE_WRITE);
  if (!out) {
    esp32p4_psram_free(buf);
    if (pcm && pcm_owned) esp32p4_psram_free(pcm);
    Serial.printf("MP4: open %s failed\n", tmp_path);
    return false;
  }
  bool wr_ok = mp4_write_all(out, ftyp.data(), ftyp.size()) &&
               mp4_write_all(out, mdat_hdr, sizeof(mdat_hdr)) &&
               mp4_write_all(out, mdat_payload.data(), mdat_payload.size()) &&
               mp4_write_all(out, moov.data(), moov.size());
  out.flush();
  out.close();
  File chk = fs.open(tmp_path, FILE_READ);
  const size_t got = chk ? chk.size() : 0;
  if (chk) chk.close();
  esp32p4_psram_free(buf);
  buf = nullptr;
  if (pcm && pcm_owned) {
    esp32p4_psram_free(pcm);
    pcm = nullptr;
  }

  if (!wr_ok || got != expect || got < 64) {
    fs.remove(tmp_path);
    fs.remove(mp4_path);
    Serial.printf("MP4: write failed (got=%u expect=%u wr=%u)\n", (unsigned)got, (unsigned)expect,
                  wr_ok ? 1u : 0u);
    return false;
  }
  if (!fs.rename(tmp_path, mp4_path)) {
    fs.remove(mp4_path);
    if (!fs.rename(tmp_path, mp4_path)) {
      Serial.printf("MP4: rename %s -> %s failed (keeping temp)\n", tmp_path, mp4_path);
      return false;
    }
  }

  float sec = movie_duration / 1000.0f;
  float afps = sec > 0.001f ? (samples.size() / (video_duration / 1000.0f)) : 0;
  Serial.printf("MP4: wrote %s  frames=%u  duration=%.2fs  avg_fps=%.1f  audio=%s (%u aac @ %u Hz)\n",
                mp4_path, (unsigned)samples.size(), sec, afps, has_audio ? "aac" : "none",
                (unsigned)aac_frames, (unsigned)pcm_rate_hz);
  mux_set_pct(progress, 100);
  return true;
}
