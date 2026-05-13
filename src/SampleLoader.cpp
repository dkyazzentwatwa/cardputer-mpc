#include "SampleLoader.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <math.h>
#include "InputMap.h"
#include "esp_heap_caps.h"

static void copyText(char* dest, size_t destSize, const char* src) {
  if (!dest || destSize == 0) return;
  if (!src) src = "";
  strlcpy(dest, src, destSize);
}

bool SampleLoader::begin(const char* root) {
  copyText(_root, sizeof(_root), root);
  initPads();
  return true;
}

void SampleLoader::initPads() {
  for (uint8_t i = 0; i < kPadCount; ++i) {
    _pads[i] = Pad();
    _pads[i].key = kPadKeys[i];
    _pads[i].sampleIndex = 255;
    _pads[i].volume = 220;
    copyText(_pads[i].label, sizeof(_pads[i].label), defaultPadLabel(i));
  }
}

void SampleLoader::clear() {
  for (uint8_t i = 0; i < _sampleCount; ++i) {
    if (_samples[i].pcm) {
      heap_caps_free(_samples[i].pcm);
    }
    _samples[i] = SampleMeta();
  }
  _sampleCount = 0;
  _sampleBytesUsed = 0;
  initPads();
}

bool SampleLoader::loadKit(const char* relativePath) {
  clear();

  char path[128];
  snprintf(path, sizeof(path), "%s/%s", _root, relativePath);
  File file = SD.open(path, FILE_READ);
  if (!file) {
    snprintf(_lastMessage, sizeof(_lastMessage), "kit missing: %s", relativePath);
    createFallbackKit(_lastMessage);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    snprintf(_lastMessage, sizeof(_lastMessage), "kit json: %s", err.c_str());
    createFallbackKit(_lastMessage);
    return false;
  }

  copyText(_kitName, sizeof(_kitName), doc["name"] | "sd-kit");
  JsonArray pads = doc["pads"].as<JsonArray>();
  uint8_t padIndex = 0;
  for (JsonObject padObj : pads) {
    if (padIndex >= kPadCount) break;
    const char* key = padObj["key"] | "";
    const char* label = padObj["label"] | defaultPadLabel(padIndex);
    const char* sample = padObj["sample"] | "";
    uint8_t volume = padObj["volume"] | 220;
    uint8_t chokeGroup = padObj["chokeGroup"] | 0;

    copyText(_pads[padIndex].label, sizeof(_pads[padIndex].label), label);
    _pads[padIndex].key = key[0] ? key[0] : kPadKeys[padIndex];
    _pads[padIndex].volume = volume;
    _pads[padIndex].chokeGroup = chokeGroup;

    if (sample[0]) {
      loadSampleForPad(padIndex, label, sample, volume);
    }
    ++padIndex;
  }

  if (_sampleCount == 0) {
    createFallbackKit("no usable samples");
    return false;
  }

  snprintf(_lastMessage, sizeof(_lastMessage), "loaded %u/%u samples", loadedCount(), _sampleCount);
  return true;
}

bool SampleLoader::loadSampleForPad(uint8_t padIndex, const char* name, const char* relativeSamplePath, uint8_t volume) {
  if (_sampleCount >= kPadCount) return false;

  char path[144];
  snprintf(path, sizeof(path), "%s/%s", _root, relativeSamplePath);

  SampleMeta& sample = _samples[_sampleCount];
  copyText(sample.name, sizeof(sample.name), name);
  copyText(sample.path, sizeof(sample.path), relativeSamplePath);
  sample.volume = volume;

  if (!loadWav(path, sample)) {
    _pads[padIndex].sampleIndex = 255;
    return false;
  }

  _pads[padIndex].sampleIndex = _sampleCount;
  ++_sampleCount;
  return true;
}

bool SampleLoader::readChunkHeader(File& file, char id[5], uint32_t& size) {
  if (file.readBytes(id, 4) != 4) return false;
  id[4] = '\0';
  if (file.read(reinterpret_cast<uint8_t*>(&size), 4) != 4) return false;
  return true;
}

bool SampleLoader::allocateSample(SampleMeta& sample, uint32_t frames) {
  uint32_t bytes = frames * sizeof(int16_t);
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (_sampleBytesUsed + bytes > kSampleHeapHardLimit || largest < bytes + kHeapSafetyMargin) {
    snprintf(sample.warning, sizeof(sample.warning), "sample too large");
    return false;
  }

  sample.pcm = static_cast<int16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
  if (!sample.pcm) {
    snprintf(sample.warning, sizeof(sample.warning), "alloc failed");
    return false;
  }

  sample.frameCount = frames;
  sample.bytes = bytes;
  _sampleBytesUsed += bytes;
  return true;
}

bool SampleLoader::loadWav(const char* absolutePath, SampleMeta& out) {
  File file = SD.open(absolutePath, FILE_READ);
  if (!file) {
    snprintf(out.warning, sizeof(out.warning), "missing wav");
    return false;
  }

  char riff[5];
  uint32_t riffSize;
  if (!readChunkHeader(file, riff, riffSize) || strcmp(riff, "RIFF") != 0) {
    snprintf(out.warning, sizeof(out.warning), "not RIFF");
    file.close();
    return false;
  }

  char wave[5] = {0};
  if (file.readBytes(wave, 4) != 4 || strcmp(wave, "WAVE") != 0) {
    snprintf(out.warning, sizeof(out.warning), "not WAVE");
    file.close();
    return false;
  }

  uint16_t audioFormat = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bits = 0;
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;

  while (file.available()) {
    char id[5];
    uint32_t size;
    if (!readChunkHeader(file, id, size)) break;
    uint32_t payloadPos = file.position();

    if (strcmp(id, "fmt ") == 0) {
      file.read(reinterpret_cast<uint8_t*>(&audioFormat), 2);
      file.read(reinterpret_cast<uint8_t*>(&channels), 2);
      file.read(reinterpret_cast<uint8_t*>(&sampleRate), 4);
      file.seek(payloadPos + 14);
      file.read(reinterpret_cast<uint8_t*>(&bits), 2);
    } else if (strcmp(id, "data") == 0) {
      dataOffset = payloadPos;
      dataSize = size;
      break;
    }

    file.seek(payloadPos + size + (size & 1));
  }

  if (audioFormat != 1 || channels != 1 || (bits != 8 && bits != 16) || (sampleRate != 16000 && sampleRate != 22050)) {
    snprintf(out.warning, sizeof(out.warning), "need PCM mono 8/16b 16k/22k");
    file.close();
    return false;
  }

  uint32_t bytesPerFrame = bits / 8;
  uint32_t frames = dataSize / bytesPerFrame;
  if (frames == 0 || !allocateSample(out, frames)) {
    file.close();
    return false;
  }

  file.seek(dataOffset);
  if (bits == 16) {
    size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(out.pcm), out.bytes);
    if (bytesRead != out.bytes) {
      snprintf(out.warning, sizeof(out.warning), "short read");
      heap_caps_free(out.pcm);
      out.pcm = nullptr;
      _sampleBytesUsed -= out.bytes;
      file.close();
      return false;
    }
  } else {
    for (uint32_t i = 0; i < frames; ++i) {
      int byte = file.read();
      if (byte < 0) {
        snprintf(out.warning, sizeof(out.warning), "short read");
        heap_caps_free(out.pcm);
        out.pcm = nullptr;
        _sampleBytesUsed -= out.bytes;
        file.close();
        return false;
      }
      out.pcm[i] = (static_cast<int16_t>(byte) - 128) << 8;
    }
  }

  file.close();
  out.sampleRate = sampleRate;
  out.bitsPerSample = bits;
  out.channels = channels;
  out.loaded = true;
  return true;
}

void SampleLoader::createFallbackKit(const char* reason) {
  clear();
  copyText(_kitName, sizeof(_kitName), "fallback");
  synthSample(0, "KICK", 'q', 82, 180, false);
  synthSample(1, "SN", 'w', 210, 120, true);
  synthSample(2, "CHH", 'e', 6200, 70, true);
  synthSample(3, "OHH", 'r', 4200, 180, true);
  snprintf(_lastMessage, sizeof(_lastMessage), "fallback: %s", reason ? reason : "no kit");
}

void SampleLoader::synthSample(uint8_t padIndex, const char* label, char key, uint32_t hz, uint16_t ms, bool noise) {
  if (_sampleCount >= kPadCount) return;
  SampleMeta& sample = _samples[_sampleCount];
  copyText(sample.name, sizeof(sample.name), label);
  copyText(sample.path, sizeof(sample.path), "generated");
  sample.sampleRate = 16000;
  sample.bitsPerSample = 16;
  sample.channels = 1;
  sample.volume = padIndex == 0 ? 240 : 210;
  sample.fallback = true;
  uint32_t frames = (sample.sampleRate * ms) / 1000;
  if (!allocateSample(sample, frames)) return;

  uint32_t noiseState = 0x12345678 + padIndex * 7919;
  for (uint32_t i = 0; i < frames; ++i) {
    float t = static_cast<float>(i) / sample.sampleRate;
    float env = 1.0f - static_cast<float>(i) / frames;
    env *= env;
    float wave = sinf(2.0f * PI * hz * t);
    if (noise) {
      noiseState = 1664525UL * noiseState + 1013904223UL;
      float n = (static_cast<int32_t>(noiseState >> 16) - 32768) / 32768.0f;
      wave = 0.35f * wave + 0.65f * n;
    }
    sample.pcm[i] = static_cast<int16_t>(wave * env * 22000.0f);
  }
  sample.loaded = true;

  Pad& pad = _pads[padIndex];
  copyText(pad.label, sizeof(pad.label), label);
  pad.key = key;
  pad.sampleIndex = _sampleCount;
  pad.volume = sample.volume;
  ++_sampleCount;
}

const SampleMeta* SampleLoader::getPadSample(uint8_t padIndex) const {
  if (padIndex >= kPadCount) return nullptr;
  uint8_t sampleIndex = _pads[padIndex].sampleIndex;
  if (sampleIndex >= _sampleCount) return nullptr;
  const SampleMeta& sample = _samples[sampleIndex];
  return sample.loaded ? &sample : nullptr;
}

uint8_t SampleLoader::loadedCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < _sampleCount; ++i) {
    if (_samples[i].loaded) ++count;
  }
  return count;
}

uint8_t SampleLoader::padForLabel(const char* label) const {
  if (!label) return 255;
  for (uint8_t i = 0; i < kPadCount; ++i) {
    if (strcasecmp(_pads[i].label, label) == 0) {
      return i;
    }
  }
  return 255;
}

uint8_t SampleLoader::padVolume(uint8_t padIndex) const {
  if (padIndex >= kPadCount) return 0;
  return _pads[padIndex].volume;
}

void SampleLoader::setPadVolume(uint8_t padIndex, uint8_t volume) {
  if (padIndex >= kPadCount) return;
  _pads[padIndex].volume = volume;
  uint8_t sampleIndex = _pads[padIndex].sampleIndex;
  if (sampleIndex < _sampleCount) {
    _samples[sampleIndex].volume = volume;
  }
  snprintf(_lastMessage, sizeof(_lastMessage), "%s vol %u", _pads[padIndex].label, volume);
}

void SampleLoader::adjustPadVolume(uint8_t padIndex, int delta) {
  if (padIndex >= kPadCount) return;
  int next = static_cast<int>(_pads[padIndex].volume) + delta;
  setPadVolume(padIndex, static_cast<uint8_t>(constrain(next, 0, 255)));
}

void SampleLoader::setMessage(const char* message) {
  copyText(_lastMessage, sizeof(_lastMessage), message);
}
