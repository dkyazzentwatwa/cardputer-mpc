#include "AudioEngine.h"

#include <M5Cardputer.h>
#include "esp_timer.h"
#include "SampleLoader.h"

void AudioEngine::begin(SampleLoader* samples) {
  _samples = samples;
  M5Cardputer.Speaker.setVolume(160);
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setAllChannelVolume(_masterVolume);
  snprintf(_lastMessage, sizeof(_lastMessage), "audio %u voices", kVoiceCount);
}

uint8_t AudioEngine::scaleVolume(uint8_t sampleVolume, uint8_t velocity) const {
  uint32_t scaled = static_cast<uint32_t>(sampleVolume) * velocity * _masterVolume / 127UL / 255UL;
  return scaled > 255 ? 255 : scaled;
}

void AudioEngine::setMasterVolume(uint8_t volume) {
  _masterVolume = volume;
  M5Cardputer.Speaker.setAllChannelVolume(_masterVolume);
  snprintf(_lastMessage, sizeof(_lastMessage), "master vol %u", _masterVolume);
}

void AudioEngine::adjustMasterVolume(int delta) {
  int next = static_cast<int>(_masterVolume) + delta;
  setMasterVolume(static_cast<uint8_t>(constrain(next, 0, 255)));
}

int8_t AudioEngine::allocateVoice(uint8_t pad) {
  for (uint8_t ch = 0; ch < kVoiceCount; ++ch) {
    if (!M5Cardputer.Speaker.isPlaying(ch)) {
      return ch;
    }
  }

  int8_t oldest = -1;
  uint32_t oldestMs = UINT32_MAX;
  for (uint8_t ch = 0; ch < kVoiceCount; ++ch) {
    if (_voices[ch].pad == 0 && pad != 0) {
      continue;
    }
    if (_voices[ch].startedMs < oldestMs) {
      oldestMs = _voices[ch].startedMs;
      oldest = ch;
    }
  }

  if (oldest < 0) oldest = 0;
  M5Cardputer.Speaker.stop(oldest);
  clearVisualVoice(oldest);
  return oldest;
}

void AudioEngine::clearVisualVoice(uint8_t channel) {
  if (channel >= kVoiceCount) return;
  _visualVoices[channel] = VisualVoice();
}

void AudioEngine::registerVisualVoice(uint8_t channel, uint8_t pad, const SampleMeta& sample, uint8_t gain, uint64_t nowUs) {
  if (channel >= kVoiceCount) return;
  VisualVoice& voice = _visualVoices[channel];
  voice.active = true;
  voice.pad = pad;
  voice.pcm = sample.pcm;
  voice.frameCount = sample.frameCount;
  voice.sampleRate = sample.sampleRate;
  voice.gain = gain;
  voice.startedUs = nowUs;
}

void AudioEngine::pruneVisualVoices(uint64_t nowUs) {
  for (uint8_t ch = 0; ch < kVoiceCount; ++ch) {
    VisualVoice& voice = _visualVoices[ch];
    if (!voice.active) continue;
    if (!M5Cardputer.Speaker.isPlaying(ch) || !voice.pcm || voice.sampleRate == 0) {
      clearVisualVoice(ch);
      continue;
    }
    uint64_t elapsedUs = nowUs > voice.startedUs ? nowUs - voice.startedUs : 0;
    uint64_t frame = (elapsedUs * voice.sampleRate) / 1000000ULL;
    if (frame >= voice.frameCount) {
      clearVisualVoice(ch);
    }
  }
}

bool AudioEngine::trigger(uint8_t pad, uint8_t velocity) {
  if (!_samples || pad >= kPadCount || velocity == 0) return false;
  const SampleMeta* sample = _samples->getPadSample(pad);
  if (!sample || !sample->loaded || !sample->pcm) {
    snprintf(_lastMessage, sizeof(_lastMessage), "pad %u empty", pad + 1);
    return false;
  }

  int8_t ch = allocateVoice(pad);
  if (ch < 0) return false;

  uint8_t gain = scaleVolume(sample->volume, velocity);
  M5Cardputer.Speaker.setChannelVolume(ch, gain);
  bool ok = M5Cardputer.Speaker.playRaw(sample->pcm, sample->frameCount, sample->sampleRate, false, 1, ch, true);
  if (ok) {
    _voices[ch].pad = pad;
    _voices[ch].startedMs = millis();
    registerVisualVoice(ch, pad, *sample, gain, esp_timer_get_time());
    ++_triggerCount;
    snprintf(_lastMessage, sizeof(_lastMessage), "pad %u ch %d", pad + 1, ch);
  } else {
    clearVisualVoice(ch);
    snprintf(_lastMessage, sizeof(_lastMessage), "trigger failed");
  }
  return ok;
}

void AudioEngine::panic() {
  M5Cardputer.Speaker.stop();
  for (auto& voice : _voices) {
    voice = Voice();
  }
  for (uint8_t ch = 0; ch < kVoiceCount; ++ch) {
    clearVisualVoice(ch);
  }
  snprintf(_lastMessage, sizeof(_lastMessage), "panic stop");
}

void AudioEngine::update() {
  pruneVisualVoices(esp_timer_get_time());
}

uint8_t AudioEngine::activeVoices() const {
  uint8_t count = 0;
  for (uint8_t ch = 0; ch < kVoiceCount; ++ch) {
    if (M5Cardputer.Speaker.isPlaying(ch)) ++count;
  }
  return count;
}
