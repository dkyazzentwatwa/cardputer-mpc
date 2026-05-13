#pragma once

#include <Arduino.h>
#include "MpcTypes.h"

class AudioEngine;
class SampleLoader;
class Sequencer;

enum class UiView : uint8_t {
  Pads,
  Steps,
  Kit,
  Project,
  Visualizer
};

class Ui {
public:
  void begin();
  void showIntro();
  void showFatal(const char* message);
  void nextView();
  void selectPad(int delta);
  void selectStep(int delta);
  uint8_t selectedPad() const { return _selectedPad; }
  uint8_t selectedStep() const { return _selectedStep; }
  UiView view() const { return _view; }
  void setStatus(const char* status);
  void force() { _dirty = true; }
  void update(uint64_t nowUs, const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio);

private:
  uint32_t liveSignature(uint64_t nowUs, const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio) const;
  void drawHeader(const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio);
  void drawPads(const SampleLoader& samples);
  void drawSteps(const Sequencer& seq, const SampleLoader& samples);
  void drawKit(const SampleLoader& samples);
  void drawProject(const Sequencer& seq, const AudioEngine& audio);
  void drawVisualizer(uint64_t nowUs, const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio);

  UiView _view = UiView::Pads;
  uint8_t _selectedPad = 0;
  uint8_t _selectedStep = 0;
  uint64_t _lastDrawUs = 0;
  uint32_t _lastSignature = 0;
  bool _dirty = true;
  char _status[80] = "ready";
};
