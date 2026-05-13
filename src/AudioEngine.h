#pragma once

#include <Arduino.h>
#include "MpcTypes.h"

class SampleLoader;

class AudioEngine {
public:
  struct VisualVoice {
    bool active = false;
    uint8_t pad = 255;
    const int16_t* pcm = nullptr;
    uint32_t frameCount = 0;
    uint32_t sampleRate = 0;
    uint8_t gain = 0;
    uint64_t startedUs = 0;
  };

  void begin(SampleLoader* samples);
  bool trigger(uint8_t pad, uint8_t velocity);
  void panic();
  void update();
  void setMasterVolume(uint8_t volume);
  void adjustMasterVolume(int delta);

  const char* lastMessage() const { return _lastMessage; }
  uint32_t triggerCount() const { return _triggerCount; }
  uint8_t activeVoices() const;
  uint8_t polyphony() const { return kVoiceCount; }
  uint8_t masterVolume() const { return _masterVolume; }
  const VisualVoice& visualVoice(uint8_t index) const { return _visualVoices[index]; }
  uint8_t visualVoiceCount() const { return kVoiceCount; }

private:
  int8_t allocateVoice(uint8_t pad);
  uint8_t scaleVolume(uint8_t sampleVolume, uint8_t velocity) const;
  void clearVisualVoice(uint8_t channel);
  void registerVisualVoice(uint8_t channel, uint8_t pad, const SampleMeta& sample, uint8_t gain, uint64_t nowUs);
  void pruneVisualVoices(uint64_t nowUs);

  struct Voice {
    uint8_t pad = 255;
    uint32_t startedMs = 0;
  };

  SampleLoader* _samples = nullptr;
  Voice _voices[kVoiceCount];
  VisualVoice _visualVoices[kVoiceCount];
  uint32_t _triggerCount = 0;
  uint8_t _masterVolume = 200;
  char _lastMessage[64] = "audio ready";
};
