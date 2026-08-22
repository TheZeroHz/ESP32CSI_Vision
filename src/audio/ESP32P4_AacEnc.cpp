#include "audio/ESP32P4_AacEnc.h"

#include <string.h>

#include <Arduino.h>

extern "C" {
#include "vo-aacenc/cmnMemory.h"
#include "vo-aacenc/voAAC.h"
}

bool esp32p4_pcm16_to_aac_lc(const int16_t *pcm, size_t pcm_samples, uint32_t rate_hz,
                             uint16_t channels, uint32_t bitrate_bps, std::vector<uint8_t> &aac,
                             std::vector<uint32_t> &frame_sizes) {
  aac.clear();
  frame_sizes.clear();
  if (!pcm || pcm_samples < 160 || rate_hz < 8000 || channels < 1 || channels > 2) return false;

  VO_AUDIO_CODECAPI api{};
  if (voGetAACEncAPI(&api) != VO_ERR_NONE) return false;

  VO_MEM_OPERATOR memOp{};
  memOp.Alloc = cmnMemAlloc;
  memOp.Copy = cmnMemCopy;
  memOp.Free = cmnMemFree;
  memOp.Set = cmnMemSet;
  memOp.Check = cmnMemCheck;

  VO_CODEC_INIT_USERDATA user{};
  user.memflag = VO_IMF_USERMEMOPERATOR;
  user.memData = &memOp;

  VO_HANDLE handle = 0;
  if (api.Init(&handle, VO_AUDIO_CodingAAC, &user) != VO_ERR_NONE) return false;

  AACENC_PARAM params{};
  params.sampleRate = (int)rate_hz;
  params.bitRate = (int)bitrate_bps;
  params.nChannels = (short)channels;
  params.adtsUsed = 0;
  if (api.SetParam(handle, VO_PID_AAC_ENCPARAM, &params) != VO_ERR_NONE) {
    api.Uninit(handle);
    return false;
  }

  const size_t frame_pcm = 1024u * (size_t)channels;
  int16_t frame[1024 * 2];
  uint8_t outbuf[2048];
  size_t off = 0;
  aac.reserve((pcm_samples / 1024 + 2) * 200);
  frame_sizes.reserve(pcm_samples / 1024 + 2);

  while (off < pcm_samples) {
    size_t n = pcm_samples - off;
    if (n >= frame_pcm) {
      memcpy(frame, pcm + off, frame_pcm * sizeof(int16_t));
      off += frame_pcm;
    } else {
      memset(frame, 0, sizeof(frame));
      memcpy(frame, pcm + off, n * sizeof(int16_t));
      off = pcm_samples;
    }

    VO_CODECBUFFER input{};
    input.Buffer = (unsigned char *)frame;
    input.Length = (VO_U32)(frame_pcm * sizeof(int16_t));
    VO_CODECBUFFER output{};
    output.Buffer = outbuf;
    output.Length = sizeof(outbuf);
    VO_AUDIO_OUTPUTINFO info{};
    if (api.SetInputData(handle, &input) != VO_ERR_NONE) {
      api.Uninit(handle);
      aac.clear();
      frame_sizes.clear();
      return false;
    }
    VO_U32 err = api.GetOutputData(handle, &output, &info);
    if (err != VO_ERR_NONE || output.Length == 0) {
      api.Uninit(handle);
      aac.clear();
      frame_sizes.clear();
      return false;
    }
    aac.insert(aac.end(), outbuf, outbuf + output.Length);
    frame_sizes.push_back(output.Length);
  }

  api.Uninit(handle);
  return !frame_sizes.empty();
}
