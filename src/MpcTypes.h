#pragma once

#include <Arduino.h>

static constexpr uint8_t kPadCount = 16;
static constexpr uint8_t kStepCount = 16;
static constexpr uint8_t kVoiceCount = 4;
static constexpr uint32_t kSampleHeapSoftLimit = 160UL * 1024UL;
static constexpr uint32_t kSampleHeapHardLimit = 220UL * 1024UL;
static constexpr uint32_t kHeapSafetyMargin = 48UL * 1024UL;
static constexpr const char* kSdRoot = "/cardputer-mpc";

struct SampleMeta {
  char name[24] = {0};
  char path[96] = {0};
  uint32_t sampleRate = 0;
  uint32_t frameCount = 0;
  uint8_t bitsPerSample = 0;
  uint8_t channels = 0;
  uint8_t volume = 220;
  bool loaded = false;
  bool fallback = false;
  int16_t* pcm = nullptr;
  uint32_t bytes = 0;
  char warning[64] = {0};
};

struct Pad {
  char label[8] = {0};
  char key = 0;
  uint8_t sampleIndex = 255;
  uint8_t volume = 220;
  uint8_t chokeGroup = 0;
};

struct Step {
  uint16_t triggerMask = 0;
  uint8_t velocity[kPadCount] = {0};
};

struct Pattern {
  char name[24] = "Pattern 1";
  uint16_t bpm = 92;
  uint8_t stepCount = kStepCount;
  Step steps[kStepCount];
};

struct Project {
  char name[32] = "cardputer-jam";
  char kitPath[96] = "kits/starter.json";
  Pattern pattern;
};
