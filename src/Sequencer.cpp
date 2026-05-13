#include "Sequencer.h"

void Sequencer::begin(uint16_t bpm) {
  clear();
  setBpm(bpm);
}

void Sequencer::clear() {
  _pattern = Pattern();
  _currentStep = 0;
  _maxJitterUs = 0;
  recalcInterval();
}

void Sequencer::recalcInterval() {
  if (_pattern.bpm < 40) _pattern.bpm = 40;
  if (_pattern.bpm > 240) _pattern.bpm = 240;
  _stepIntervalUs = 60000000ULL / _pattern.bpm / 4ULL;
}

void Sequencer::setBpm(uint16_t bpm) {
  _pattern.bpm = constrain(bpm, 40, 240);
  recalcInterval();
}

void Sequencer::adjustBpm(int delta) {
  int next = static_cast<int>(_pattern.bpm) + delta;
  setBpm(static_cast<uint16_t>(constrain(next, 40, 240)));
}

void Sequencer::start(uint64_t nowUs) {
  _playing = true;
  _currentStep = 0;
  _nextStepUs = nowUs;
  _maxJitterUs = 0;
}

void Sequencer::togglePlay(uint64_t nowUs) {
  if (_playing) {
    stop();
  } else {
    start(nowUs);
  }
}

void Sequencer::stop() {
  _playing = false;
  _recording = false;
  _currentStep = 0;
}

void Sequencer::update(uint64_t nowUs, StepTriggerCallback callback) {
  if (!_playing || !callback) return;
  if (nowUs < _nextStepUs) return;

  uint64_t late = nowUs - _nextStepUs;
  if (late > _maxJitterUs && late < 1000000ULL) {
    _maxJitterUs = static_cast<uint32_t>(late);
  }

  fireStep(_currentStep, callback);
  _currentStep = (_currentStep + 1) % _pattern.stepCount;
  _nextStepUs += _stepIntervalUs;
  if (nowUs > _nextStepUs + _stepIntervalUs) {
    _nextStepUs = nowUs + _stepIntervalUs;
  }
}

void Sequencer::fireStep(uint8_t step, StepTriggerCallback callback) {
  if (step >= kStepCount) return;
  const Step& s = _pattern.steps[step];
  for (uint8_t pad = 0; pad < kPadCount; ++pad) {
    if (s.triggerMask & (1U << pad)) {
      uint8_t vel = s.velocity[pad] ? s.velocity[pad] : 110;
      callback(pad, vel);
    }
  }
}

void Sequencer::toggleStep(uint8_t step, uint8_t pad, uint8_t velocity) {
  if (step >= kStepCount || pad >= kPadCount) return;
  Step& s = _pattern.steps[step];
  uint16_t bit = 1U << pad;
  if (s.triggerMask & bit) {
    s.triggerMask &= ~bit;
    s.velocity[pad] = 0;
  } else {
    s.triggerMask |= bit;
    s.velocity[pad] = velocity;
  }
}

bool Sequencer::recordHit(uint8_t pad, uint8_t velocity) {
  if (!_recording || pad >= kPadCount) return false;
  Step& s = _pattern.steps[_currentStep];
  s.triggerMask |= 1U << pad;
  s.velocity[pad] = velocity;
  return true;
}

void Sequencer::clearStep(uint8_t step) {
  if (step >= kStepCount) return;
  _pattern.steps[step] = Step();
}

void Sequencer::setPattern(const Pattern& pattern) {
  _pattern = pattern;
  if (_pattern.stepCount == 0 || _pattern.stepCount > kStepCount) {
    _pattern.stepCount = kStepCount;
  }
  recalcInterval();
}

bool Sequencer::hasEvents() const {
  for (uint8_t step = 0; step < _pattern.stepCount && step < kStepCount; ++step) {
    if (_pattern.steps[step].triggerMask) return true;
  }
  return false;
}
