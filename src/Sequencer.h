#pragma once

#include <Arduino.h>
#include "MpcTypes.h"

typedef void (*StepTriggerCallback)(uint8_t pad, uint8_t velocity);

class Sequencer {
public:
  void begin(uint16_t bpm);
  void update(uint64_t nowUs, StepTriggerCallback callback);
  void start(uint64_t nowUs);
  void togglePlay(uint64_t nowUs);
  void stop();
  void toggleRecord() { _recording = !_recording; }
  void setRecording(bool recording) { _recording = recording; }
  void setBpm(uint16_t bpm);
  void adjustBpm(int delta);
  void clear();
  void clearStep(uint8_t step);
  void toggleStep(uint8_t step, uint8_t pad, uint8_t velocity);
  bool recordHit(uint8_t pad, uint8_t velocity);
  void setPattern(const Pattern& pattern);
  const Pattern& pattern() const { return _pattern; }
  Pattern& pattern() { return _pattern; }

  bool playing() const { return _playing; }
  bool recording() const { return _recording; }
  uint8_t currentStep() const { return _currentStep; }
  uint16_t bpm() const { return _pattern.bpm; }
  uint64_t stepIntervalUs() const { return _stepIntervalUs; }
  uint32_t maxJitterUs() const { return _maxJitterUs; }
  bool hasEvents() const;

private:
  void fireStep(uint8_t step, StepTriggerCallback callback);
  void recalcInterval();

  Pattern _pattern;
  bool _playing = false;
  bool _recording = false;
  uint8_t _currentStep = 0;
  uint64_t _nextStepUs = 0;
  uint64_t _stepIntervalUs = 0;
  uint32_t _maxJitterUs = 0;
};
