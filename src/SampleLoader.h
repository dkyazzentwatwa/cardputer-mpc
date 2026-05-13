#pragma once

#include <Arduino.h>
#include <FS.h>
#include "MpcTypes.h"

class SampleLoader {
public:
  bool begin(const char* root);
  bool loadKit(const char* relativePath);
  void createFallbackKit(const char* reason);
  void clear();

  const SampleMeta* getPadSample(uint8_t pad) const;
  const Pad& pad(uint8_t index) const { return _pads[index]; }
  const SampleMeta& sample(uint8_t index) const { return _samples[index]; }
  uint8_t sampleCount() const { return _sampleCount; }
  const char* kitName() const { return _kitName; }
  const char* lastMessage() const { return _lastMessage; }
  uint32_t sampleBytesUsed() const { return _sampleBytesUsed; }
  uint8_t loadedCount() const;
  uint8_t padForLabel(const char* label) const;
  uint8_t padVolume(uint8_t padIndex) const;
  void setPadVolume(uint8_t padIndex, uint8_t volume);
  void adjustPadVolume(uint8_t padIndex, int delta);

private:
  bool loadSampleForPad(uint8_t padIndex, const char* name, const char* relativeSamplePath, uint8_t volume);
  bool loadWav(const char* absolutePath, SampleMeta& out);
  bool readChunkHeader(File& file, char id[5], uint32_t& size);
  bool allocateSample(SampleMeta& sample, uint32_t frames);
  void setMessage(const char* message);
  void initPads();
  void synthSample(uint8_t padIndex, const char* label, char key, uint32_t hz, uint16_t ms, bool noise);

  char _root[64] = {0};
  char _kitName[32] = "fallback";
  char _lastMessage[96] = "boot";
  SampleMeta _samples[kPadCount];
  Pad _pads[kPadCount];
  uint8_t _sampleCount = 0;
  uint32_t _sampleBytesUsed = 0;
};
