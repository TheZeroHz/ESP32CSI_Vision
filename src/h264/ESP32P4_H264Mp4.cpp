#include "h264/ESP32P4_H264Mp4.h"

#include <Arduino.h>
#include <string.h>
#include <vector>

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
    else if (i + 4 < len && buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 0 && buf[i + 3] == 1) sc = 4;
    if (!sc) {
      i++;
      continue;
    }
    size_t start = i + sc;
    size_t j = start;
    while (j + 3 < len) {
      if (buf[j] == 0 && buf[j + 1] == 0 && (buf[j + 2] == 1 || (j + 4 <= len && buf[j + 2] == 0 && buf[j + 3] == 1))) break;
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

bool esp32p4_h264_annexb_to_mp4(fs::FS &fs, const char *h264_path, const char *mp4_path, uint16_t width,
                                uint16_t height, uint32_t duration_ms) {
  if (!h264_path || !mp4_path) return false;
  if (duration_ms < 1) duration_ms = 1;

  File in = fs.open(h264_path, FILE_READ);
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
    if (n.type == 7) {  // SPS
      sps = n.data;
      sps_len = n.len;
    } else if (n.type == 8) {  // PPS
      pps = n.data;
      pps_len = n.len;
    } else if (n.type == 1 || n.type == 5) {  // slice / IDR
      samples.push_back(n);
    }
  }

  if (!sps || !pps || samples.empty()) {
    esp32p4_psram_free(buf);
    Serial.printf("MP4: need SPS/PPS/slices (sps=%u pps=%u samples=%u)\n", sps ? 1u : 0u, pps ? 1u : 0u,
                  (unsigned)samples.size());
    return false;
  }

  // Build mdat payload (length-prefixed NALs)
  std::vector<uint8_t> mdat_payload;
  mdat_payload.reserve(sz);
  std::vector<uint32_t> sample_sizes;
  sample_sizes.reserve(samples.size());
  for (const auto &s : samples) {
    w32(mdat_payload, s.len);
    wbytes(mdat_payload, s.data, s.len);
    sample_sizes.push_back(4 + s.len);
  }

  // avcC
  std::vector<uint8_t> avcC;
  avcC.push_back(1);
  avcC.push_back(sps[1]);
  avcC.push_back(sps[2]);
  avcC.push_back(sps[3]);
  avcC.push_back(0xFF);  // lengthSizeMinusOne = 3
  avcC.push_back(0xE1);  // one SPS
  w16(avcC, (uint16_t)sps_len);
  wbytes(avcC, sps, sps_len);
  avcC.push_back(1);  // one PPS
  w16(avcC, (uint16_t)pps_len);
  wbytes(avcC, pps, pps_len);

  const uint32_t timescale = 1000;  // milliseconds
  const uint32_t duration = duration_ms;
  const uint32_t sample_delta =
      samples.size() ? (uint32_t)((duration_ms + samples.size() - 1) / samples.size()) : 1;
  const uint32_t matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};

  // ftyp first so we know mdat data offset for stco
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
  const uint32_t data_offset = (uint32_t)ftyp.size() + 8;  // after ftyp + mdat header

  // mdhd
  std::vector<uint8_t> mdhd;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 0);
    w32(body, 0);
    w32(body, timescale);
    w32(body, duration);
    w16(body, 0x55C4);
    w16(body, 0);
    box(mdhd, "mdhd", body);
  }

  // hdlr
  std::vector<uint8_t> hdlr;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 0);
    wfourcc(body, "vide");
    w32(body, 0);
    w32(body, 0);
    w32(body, 0);
    const char *name = "VideoHandler";
    wbytes(body, (const uint8_t *)name, strlen(name) + 1);
    box(hdlr, "hdlr", body);
  }

  // vmhd
  std::vector<uint8_t> vmhd;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(1);  // flags
    w16(body, 0);
    w16(body, 0);
    w16(body, 0);
    w16(body, 0);
    box(vmhd, "vmhd", body);
  }

  // dinf/dref/url
  std::vector<uint8_t> dinf;
  {
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
    box(dinf, "dinf", dref);
  }

  // stsd / avc1
  std::vector<uint8_t> stsd;
  {
    std::vector<uint8_t> avcC_box;
    box(avcC_box, "avcC", avcC);

    std::vector<uint8_t> avc1_body;
    for (int i = 0; i < 6; i++) avc1_body.push_back(0);  // reserved
    w16(avc1_body, 1);                                   // data ref index
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
    for (int i = 0; i < 32; i++) avc1_body.push_back(0);  // compressor name
    w16(avc1_body, 0x0018);
    w16(avc1_body, 0xFFFF);
    wbytes(avc1_body, avcC_box.data(), avcC_box.size());

    std::vector<uint8_t> avc1;
    box(avc1, "avc1", avc1_body);

    std::vector<uint8_t> stsd_body;
    stsd_body.push_back(0);
    stsd_body.push_back(0);
    stsd_body.push_back(0);
    stsd_body.push_back(0);
    w32(stsd_body, 1);
    wbytes(stsd_body, avc1.data(), avc1.size());
    box(stsd, "stsd", stsd_body);
  }

  // stts — one entry: every sample lasts sample_delta ms
  std::vector<uint8_t> stts;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 1);
    w32(body, (uint32_t)samples.size());
    w32(body, sample_delta ? sample_delta : 1);
    box(stts, "stts", body);
  }

  // stss (IDR sync samples)
  std::vector<uint8_t> stss;
  {
    std::vector<uint32_t> sync;
    for (size_t i = 0; i < samples.size(); i++) {
      if (samples[i].type == 5) sync.push_back((uint32_t)(i + 1));
    }
    if (sync.empty()) sync.push_back(1);
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, (uint32_t)sync.size());
    for (uint32_t s : sync) w32(body, s);
    box(stss, "stss", body);
  }

  // stsc
  std::vector<uint8_t> stsc;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 1);
    w32(body, 1);
    w32(body, (uint32_t)samples.size());
    w32(body, 1);
    box(stsc, "stsc", body);
  }

  // stsz
  std::vector<uint8_t> stsz;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 0);
    w32(body, (uint32_t)sample_sizes.size());
    for (uint32_t s : sample_sizes) w32(body, s);
    box(stsz, "stsz", body);
  }

  // stco
  std::vector<uint8_t> stco;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 1);
    w32(body, data_offset);
    box(stco, "stco", body);
  }

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
  wbytes(mdia_payload, mdhd.data(), mdhd.size());
  wbytes(mdia_payload, hdlr.data(), hdlr.size());
  wbytes(mdia_payload, minf.data(), minf.size());
  std::vector<uint8_t> mdia;
  box(mdia, "mdia", mdia_payload);

  // tkhd
  std::vector<uint8_t> tkhd;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(3);  // flags track enabled+in movie
    w32(body, 0);
    w32(body, 0);
    w32(body, 1);  // track id
    w32(body, 0);
    w32(body, duration);
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

  // mvhd
  std::vector<uint8_t> mvhd;
  {
    std::vector<uint8_t> body;
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
    w32(body, 0);
    w32(body, 0);
    w32(body, timescale);
    w32(body, duration);
    w32(body, 0x00010000);
    w16(body, 0x0100);
    w16(body, 0);
    w32(body, 0);
    w32(body, 0);
    for (uint32_t m : matrix) w32(body, m);
    for (int i = 0; i < 6; i++) w32(body, 0);
    w32(body, 2);  // next track id
    box(mvhd, "mvhd", body);
  }

  std::vector<uint8_t> moov_payload;
  wbytes(moov_payload, mvhd.data(), mvhd.size());
  wbytes(moov_payload, trak.data(), trak.size());
  std::vector<uint8_t> moov;
  box(moov, "moov", moov_payload);

  // mdat
  std::vector<uint8_t> mdat;
  w32(mdat, (uint32_t)(8 + mdat_payload.size()));
  wfourcc(mdat, "mdat");
  wbytes(mdat, mdat_payload.data(), mdat_payload.size());

  File out = fs.open(mp4_path, FILE_WRITE);
  if (!out) {
    esp32p4_psram_free(buf);
    Serial.printf("MP4: open %s failed\n", mp4_path);
    return false;
  }
  out.write(ftyp.data(), ftyp.size());
  out.write(mdat.data(), mdat.size());
  out.write(moov.data(), moov.size());
  out.close();
  esp32p4_psram_free(buf);

  float sec = duration_ms / 1000.0f;
  float afps = sec > 0.001f ? (samples.size() / sec) : 0;
  Serial.printf("MP4: wrote %s  frames=%u  duration=%.2fs  avg_fps=%.1f\n", mp4_path,
                (unsigned)samples.size(), sec, afps);
  return true;
}
