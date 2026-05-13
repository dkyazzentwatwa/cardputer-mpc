#include "Ui.h"

#include <M5Cardputer.h>
#include "AudioEngine.h"
#include "InputMap.h"
#include "SampleLoader.h"
#include "Sequencer.h"
#include "esp_heap_caps.h"

namespace {
static constexpr uint8_t kViewCount = 5;
static constexpr int kScreenW = 240;
static constexpr int kScreenH = 135;
static constexpr int kHeaderH = 26;
static constexpr int kFooterY = 121;
static constexpr uint32_t kIntroSplashMs = 4000;
static constexpr uint32_t kIntroSkipDebounceMs = 650;
static constexpr uint16_t kBg = TFT_BLACK;
static constexpr uint16_t kChrome = TFT_NAVY;
static constexpr uint16_t kPanel = TFT_DARKCYAN;
static constexpr uint16_t kText = TFT_WHITE;
static constexpr uint16_t kMuted = TFT_LIGHTGREY;
static constexpr uint16_t kDim = TFT_DARKGREY;
static constexpr uint16_t kFocus = TFT_YELLOW;
static constexpr uint16_t kAccent = TFT_CYAN;
static constexpr uint16_t kHit = TFT_GREEN;
static constexpr uint16_t kMotion = TFT_ORANGE;
static constexpr uint16_t kRecord = TFT_RED;

const char* viewLabel(UiView view) {
  switch (view) {
    case UiView::Pads: return "PAD";
    case UiView::Steps: return "SEQ";
    case UiView::Kit: return "KIT";
    case UiView::Project: return "PRJ";
    case UiView::Visualizer: return "WAV";
  }
  return "---";
}

uint8_t liveVisualCount(const AudioEngine& audio) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < audio.visualVoiceCount(); ++i) {
    if (audio.visualVoice(i).active) ++count;
  }
  return count;
}

void drawLabel(int x, int y, const char* label, uint16_t color = kAccent) {
  M5Cardputer.Display.setTextColor(color, kBg);
  M5Cardputer.Display.setCursor(x, y);
  M5Cardputer.Display.print(label);
}

void drawMeter(int x, int y, int w, int h, uint32_t value, uint32_t maxValue, uint16_t color) {
  M5Cardputer.Display.drawRect(x, y, w, h, kDim);
  uint32_t safeMax = maxValue ? maxValue : 1;
  int fill = static_cast<int>((static_cast<uint64_t>(value) * (w - 2)) / safeMax);
  fill = constrain(fill, 0, w - 2);
  if (fill > 0) {
    M5Cardputer.Display.fillRect(x + 1, y + 1, fill, h - 2, color);
  }
}

bool introSkipPressed() {
  if (!M5Cardputer.Keyboard.isPressed()) return false;
  Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
  return state.enter || state.space || state.tab;
}

template <typename GfxT>
void drawIntroFrame(GfxT& gfx, int16_t width, int16_t height, uint32_t elapsed) {
  static const uint8_t dotX[] = {8, 23, 41, 64, 88, 113, 139, 166, 190, 215, 231};
  static const uint8_t dotY[] = {31, 92, 17, 111, 49, 76, 27, 103, 59, 21, 88};
  static const uint8_t dotSpeed[] = {1, 2, 1, 3, 2, 1, 2, 3, 1, 2, 1};
  const int16_t sweep = (elapsed / 16) % height;
  const int16_t pulse = (elapsed / 85) % 22;
  const int16_t progress = min((int16_t)(width - 44),
                               (int16_t)(((width - 44) * elapsed) / kIntroSplashMs));

  gfx.fillScreen(TFT_BLACK);
  gfx.fillRect(0, 0, width, height, 0x0008);

  for (int16_t y = 10; y < height; y += 16) {
    gfx.drawFastHLine(12, y, width - 24, 0x0841);
  }
  for (int16_t x = 18; x < width; x += 24) {
    gfx.drawFastVLine(x, 8, height - 16, 0x0821);
  }

  for (uint8_t i = 0; i < sizeof(dotX); ++i) {
    const int16_t x = (dotX[i] + (elapsed / (42 / dotSpeed[i]))) % width;
    const int16_t y = dotY[i];
    const uint16_t color = (i % 3 == 0) ? kHit : ((i % 3 == 1) ? kAccent : kMotion);
    gfx.drawPixel(x, y, color);
    if ((i % 4) == 0) gfx.drawPixel((x + 1) % width, y, color);
  }

  gfx.drawRect(5, 5, width - 10, height - 10, kPanel);
  gfx.drawRect(8, 8, width - 16, height - 16, kAccent);
  gfx.drawFastHLine(12, 12, 34 + pulse, kFocus);
  gfx.drawFastHLine(width - 46 - pulse, 12, 34 + pulse, kFocus);
  gfx.drawFastHLine(12, height - 13, 34 + pulse, kAccent);
  gfx.drawFastHLine(width - 46 - pulse, height - 13, 34 + pulse, kAccent);
  gfx.drawFastVLine(12, 12, 24, kAccent);
  gfx.drawFastVLine(width - 13, 12, 24, kAccent);
  gfx.drawFastVLine(12, height - 36, 24, kAccent);
  gfx.drawFastVLine(width - 13, height - 36, 24, kAccent);

  gfx.fillRect(10, sweep, width - 20, 2, 0x07FF);
  if (sweep > 0) gfx.drawFastHLine(14, sweep - 1, width - 28, 0x03E0);

  const int16_t waveX = 23;
  const int16_t waveY = 91;
  const int16_t waveW = width - 46;
  const int16_t mid = waveY + 12;
  gfx.drawRect(waveX - 2, waveY - 2, waveW + 4, 28, kPanel);
  gfx.drawFastHLine(waveX, mid, waveW, kDim);
  int16_t lastX = waveX;
  int16_t lastY = mid;
  for (int16_t x = 0; x < waveW; x += 4) {
    const int16_t phase = (x + elapsed / 9) % 48;
    int16_t amp = phase < 24 ? phase - 12 : 36 - phase;
    amp = constrain(amp, -11, 11);
    const int16_t y = mid - amp;
    gfx.drawLine(lastX, lastY, waveX + x, y, (x % 16 == 0) ? kHit : kAccent);
    lastX = waveX + x;
    lastY = y;
  }
  const int16_t playhead = waveX + ((elapsed / 18) % waveW);
  gfx.drawFastVLine(playhead, waveY - 1, 28, kMotion);
  gfx.fillCircle(playhead, mid, 3, kFocus);

  gfx.setTextSize(2);
  gfx.setTextColor(TFT_BLACK, TFT_BLACK);
  gfx.setCursor(37, 37);
  gfx.print("Cypher Tune");
  gfx.setTextColor(kHit, TFT_BLACK);
  gfx.setCursor(35, 35);
  gfx.print("Cypher Tune");

  gfx.setTextSize(1);
  gfx.setTextColor(kMuted, TFT_BLACK);
  gfx.setCursor(70, 61);
  gfx.print("by littlehakr");
  gfx.setTextColor(kAccent, TFT_BLACK);
  gfx.setCursor(52, 74);
  gfx.print("CARDPUTER MPC ONLINE");

  gfx.drawRect(21, height - 25, width - 42, 8, kPanel);
  gfx.fillRect(23, height - 23, progress, 4, kHit);
  gfx.fillRect(23 + progress, height - 23, 3, 4, kText);
}
}  // namespace

void Ui::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextWrap(false);
  M5Cardputer.Display.setTextColor(kText, kBg);
  M5Cardputer.Display.fillScreen(kBg);
  _dirty = true;
}

void Ui::showIntro() {
  auto& display = M5Cardputer.Display;
  const int16_t width = display.width();
  const int16_t height = display.height();
  const uint32_t started = millis();
  M5Canvas frame;
  frame.setColorDepth(16);
  const bool buffered = frame.createSprite(width, height) != nullptr;

  Serial.println("cypher tune intro start");
  display.setRotation(1);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextDatum(top_left);
  if (buffered) {
    frame.setTextFont(1);
    frame.setTextDatum(top_left);
    frame.setTextWrap(false);
  }

  while (millis() - started < kIntroSplashMs) {
    const uint32_t elapsed = millis() - started;
    M5Cardputer.update();
    if (elapsed > kIntroSkipDebounceMs && introSkipPressed()) break;
    if (buffered) {
      drawIntroFrame(frame, width, height, elapsed);
      frame.pushSprite(&display, 0, 0);
    } else {
      drawIntroFrame(display, width, height, elapsed);
    }
    delay(33);
    yield();
  }

  if (buffered) frame.deleteSprite();
  display.setRotation(1);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextDatum(top_left);
  display.fillScreen(kBg);
  _dirty = true;
  Serial.println("cypher tune intro done");
}

void Ui::showFatal(const char* message) {
  M5Cardputer.Display.fillScreen(kBg);
  M5Cardputer.Display.setTextColor(kRecord, kBg);
  M5Cardputer.Display.setCursor(6, 8);
  M5Cardputer.Display.println("Cardputer MPC");
  M5Cardputer.Display.setTextColor(kText, kBg);
  M5Cardputer.Display.println();
  M5Cardputer.Display.println(message ? message : "fatal");
}

void Ui::nextView() {
  _view = static_cast<UiView>((static_cast<uint8_t>(_view) + 1) % kViewCount);
  _dirty = true;
}

void Ui::selectPad(int delta) {
  int next = static_cast<int>(_selectedPad) + delta;
  while (next < 0) next += kPadCount;
  _selectedPad = next % kPadCount;
  _dirty = true;
}

void Ui::selectStep(int delta) {
  int next = static_cast<int>(_selectedStep) + delta;
  while (next < 0) next += kStepCount;
  _selectedStep = next % kStepCount;
  _dirty = true;
}

void Ui::setStatus(const char* status) {
  strlcpy(_status, status ? status : "", sizeof(_status));
  _dirty = true;
}

uint32_t Ui::liveSignature(uint64_t nowUs, const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio) const {
  uint32_t hash = 2166136261UL;
  const char* status = _status;
  while (*status) {
    hash ^= static_cast<uint8_t>(*status++);
    hash *= 16777619UL;
  }
  hash ^= static_cast<uint32_t>(_view) << 28;
  hash ^= static_cast<uint32_t>(_selectedPad) << 24;
  hash ^= static_cast<uint32_t>(_selectedStep) << 20;
  hash ^= static_cast<uint32_t>(seq.currentStep()) << 16;
  hash ^= static_cast<uint32_t>(seq.bpm()) << 4;
  hash ^= seq.playing() ? 0x01UL : 0;
  hash ^= seq.recording() ? 0x02UL : 0;
  hash ^= static_cast<uint32_t>(audio.masterVolume()) << 8;
  hash ^= static_cast<uint32_t>(audio.activeVoices()) << 2;
  hash ^= static_cast<uint32_t>(samples.padVolume(_selectedPad));
  hash ^= static_cast<uint32_t>(audio.triggerCount());
  for (uint8_t i = 0; i < audio.visualVoiceCount(); ++i) {
    const AudioEngine::VisualVoice& voice = audio.visualVoice(i);
    if (!voice.active) continue;
    hash ^= static_cast<uint32_t>(voice.pad + 1) << (i * 3);
    hash ^= static_cast<uint32_t>(voice.gain) << i;
  }
  if (_view == UiView::Visualizer && (seq.playing() || seq.recording() || liveVisualCount(audio) > 0)) {
    hash ^= static_cast<uint32_t>((nowUs / 50000ULL) & 0xFFFFUL);
  }
  return hash;
}

void Ui::update(uint64_t nowUs, const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio) {
  uint32_t signature = liveSignature(nowUs, seq, samples, audio);
  if (!_dirty && signature == _lastSignature) return;
  if (!_dirty && nowUs - _lastDrawUs < 30000ULL) return;
  _lastDrawUs = nowUs;
  _lastSignature = signature;
  _dirty = false;

  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillScreen(kBg);
  drawHeader(seq, samples, audio);
  switch (_view) {
    case UiView::Pads: drawPads(samples); break;
    case UiView::Steps: drawSteps(seq, samples); break;
    case UiView::Kit: drawKit(samples); break;
    case UiView::Project: drawProject(seq, audio); break;
    case UiView::Visualizer: drawVisualizer(nowUs, seq, samples, audio); break;
  }
  M5Cardputer.Display.endWrite();
}

void Ui::drawHeader(const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio) {
  uint16_t color = seq.playing() ? kHit : kDim;
  M5Cardputer.Display.fillRect(0, 0, kScreenW, kHeaderH, kChrome);
  M5Cardputer.Display.setTextColor(kText, kChrome);
  M5Cardputer.Display.setCursor(4, 3);
  M5Cardputer.Display.printf("%s %03uB M%u P%u",
                             seq.playing() ? "PLAY" : "STOP",
                             seq.bpm(),
                             audio.masterVolume(),
                             samples.padVolume(_selectedPad));
  M5Cardputer.Display.fillCircle(225, 7, 4, color);
  if (seq.recording()) {
    M5Cardputer.Display.fillRect(190, 2, 24, 11, kRecord);
    M5Cardputer.Display.setTextColor(kText, kRecord);
    M5Cardputer.Display.setCursor(194, 4);
    M5Cardputer.Display.print("REC");
  }

  for (uint8_t i = 0; i < kViewCount; ++i) {
    UiView view = static_cast<UiView>(i);
    int x = i * 48;
    bool active = view == _view;
    uint16_t fill = active ? kPanel : kChrome;
    uint16_t text = active ? kText : kMuted;
    M5Cardputer.Display.fillRect(x, 16, 48, 10, fill);
    if (active) M5Cardputer.Display.drawFastHLine(x + 2, 25, 44, kFocus);
    M5Cardputer.Display.setTextColor(text, fill);
    M5Cardputer.Display.setCursor(x + 14, 18);
    M5Cardputer.Display.print(viewLabel(view));
  }

  M5Cardputer.Display.fillRect(0, kFooterY, kScreenW, kScreenH - kFooterY, kBg);
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(4, 124);
  M5Cardputer.Display.printf("%.31s", _status);
  M5Cardputer.Display.setCursor(198, 124);
  M5Cardputer.Display.printf("V:%u/%u", audio.activeVoices(), audio.polyphony());
}

void Ui::drawPads(const SampleLoader& samples) {
  drawLabel(4, 31, "PADS");
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(36, 31);
  M5Cardputer.Display.printf("%s  SEL %s", samples.kitName(), samples.pad(_selectedPad).label);
  for (uint8_t i = 0; i < kPadCount; ++i) {
    uint8_t col = i % 4;
    uint8_t row = i / 4;
    int x = 5 + col * 59;
    int y = 45 + row * 18;
    bool selected = i == _selectedPad;
    bool loaded = samples.getPadSample(i) != nullptr;
    uint16_t outline = selected ? kFocus : (loaded ? kAccent : kDim);
    uint16_t fill = selected ? kPanel : (loaded ? TFT_BLACK : TFT_DARKGREY);
    M5Cardputer.Display.fillRect(x, y, 54, 15, fill);
    M5Cardputer.Display.drawRect(x, y, 54, 15, outline);
    M5Cardputer.Display.fillRect(x + 2, y + 2, 8, 11, selected ? kFocus : (loaded ? kHit : kDim));
    M5Cardputer.Display.setTextColor(selected ? kText : (loaded ? kMuted : kBg), fill);
    M5Cardputer.Display.setCursor(x + 13, y + 4);
    M5Cardputer.Display.printf("%c %s", samples.pad(i).key, samples.pad(i).label);
  }
}

void Ui::drawSteps(const Sequencer& seq, const SampleLoader& samples) {
  drawLabel(4, 31, "STEPS");
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(44, 31);
  M5Cardputer.Display.printf("STEP %02u  PAD %s  VOL %u",
                             _selectedStep + 1,
                             samples.pad(_selectedPad).label,
                             samples.padVolume(_selectedPad));

  for (uint8_t step = 0; step < kStepCount; ++step) {
    int x = 7 + step * 14;
    int y = 51;
    if ((step % 4) == 0) {
      M5Cardputer.Display.drawFastVLine(x - 3, y - 6, 42, kPanel);
    }
    bool hasHit = seq.pattern().steps[step].triggerMask & (1U << _selectedPad);
    uint8_t velocity = seq.pattern().steps[step].velocity[_selectedPad];
    int height = hasHit ? map(velocity ? velocity : 80, 0, 127, 8, 32) : 5;
    uint16_t color = hasHit ? kHit : kDim;
    if (step == seq.currentStep() && seq.playing()) color = kMotion;
    if (step == _selectedStep) {
      M5Cardputer.Display.drawRect(x - 2, y - 5, 12, 42, kFocus);
    }
    M5Cardputer.Display.fillRect(x, y + 32 - height, 8, height, color);
    M5Cardputer.Display.drawFastHLine(x, y + 35, 8, kDim);
  }

  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(4, 94);
  M5Cardputer.Display.print("5/6 step  ,/. pad  key toggles");
  M5Cardputer.Display.setCursor(4, 107);
  M5Cardputer.Display.printf("play %02u  jitter %lu us", seq.currentStep() + 1, static_cast<unsigned long>(seq.maxJitterUs()));
}

void Ui::drawKit(const SampleLoader& samples) {
  drawLabel(4, 31, "KIT");
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(30, 31);
  M5Cardputer.Display.printf("Kit %s %u/%u", samples.kitName(), samples.loadedCount(), samples.sampleCount());
  for (uint8_t i = 0; i < 6 && i < samples.sampleCount(); ++i) {
    const SampleMeta& s = samples.sample(i);
    int y = 45 + i * 12;
    M5Cardputer.Display.drawFastHLine(4, y + 10, 232, kPanel);
    M5Cardputer.Display.fillRect(5, y + 2, 5, 5, s.loaded ? kHit : kRecord);
    M5Cardputer.Display.setTextColor(kText, kBg);
    M5Cardputer.Display.setCursor(14, y);
    M5Cardputer.Display.printf("%-7s", s.name);
    M5Cardputer.Display.setTextColor(kMuted, kBg);
    M5Cardputer.Display.setCursor(86, y);
    M5Cardputer.Display.printf("%luHz", static_cast<unsigned long>(s.sampleRate));
    M5Cardputer.Display.setCursor(150, y);
    M5Cardputer.Display.printf("%luK", static_cast<unsigned long>(s.bytes / 1024));
    if (s.warning[0]) {
      M5Cardputer.Display.setTextColor(kRecord, kBg);
      M5Cardputer.Display.setCursor(190, y);
      M5Cardputer.Display.print("WARN");
    }
  }
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(4, 112);
  M5Cardputer.Display.printf("RAM %luK", static_cast<unsigned long>(samples.sampleBytesUsed() / 1024));
  drawMeter(54, 111, 96, 8, samples.sampleBytesUsed(), kSampleHeapSoftLimit, kAccent);
  M5Cardputer.Display.setCursor(158, 112);
  M5Cardputer.Display.printf("%.13s", samples.lastMessage());
}

void Ui::drawProject(const Sequencer& seq, const AudioEngine& audio) {
  drawLabel(4, 31, "PROJECT");
  const char* labels[] = {"SAVE", "LOAD", "NEW", "PANIC", "PLAY", "REC", "BPM", "VOL"};
  const char* keys[] = {"Sh+S", "L", "N", "`/BS", "Space", "Enter", "_/+", "-/="};
  for (uint8_t i = 0; i < 8; ++i) {
    int col = i % 2;
    int row = i / 2;
    int x = 6 + col * 118;
    int y = 47 + row * 16;
    uint16_t fill = (i == 5 && seq.recording()) ? kRecord : ((i == 4 && seq.playing()) ? kPanel : kBg);
    M5Cardputer.Display.drawRect(x, y, 110, 13, (i == 4 && seq.playing()) ? kHit : kDim);
    if (fill != kBg) M5Cardputer.Display.fillRect(x + 1, y + 1, 108, 11, fill);
    M5Cardputer.Display.setTextColor(kText, fill);
    M5Cardputer.Display.setCursor(x + 4, y + 3);
    M5Cardputer.Display.print(labels[i]);
    M5Cardputer.Display.setTextColor(kMuted, fill);
    M5Cardputer.Display.setCursor(x + 58, y + 3);
    M5Cardputer.Display.print(keys[i]);
  }
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(4, 113);
  M5Cardputer.Display.printf("trig %lu  interval %lu us",
                             static_cast<unsigned long>(audio.triggerCount()),
                             static_cast<unsigned long>(seq.stepIntervalUs()));
}

void Ui::drawVisualizer(uint64_t nowUs, const Sequencer& seq, const SampleLoader& samples, const AudioEngine& audio) {
  drawLabel(4, 31, "WAVEFORM");
  M5Cardputer.Display.setTextColor(kMuted, kBg);
  M5Cardputer.Display.setCursor(66, 31);
  M5Cardputer.Display.printf("%s  PAD %s", seq.playing() ? "LIVE" : "ARM", samples.pad(_selectedPad).label);

  static constexpr int x0 = 6;
  static constexpr int y0 = 45;
  static constexpr int w = 228;
  static constexpr int h = 62;
  static constexpr int mid = y0 + h / 2;
  M5Cardputer.Display.drawRect(x0 - 1, y0 - 1, w + 2, h + 2, kPanel);
  M5Cardputer.Display.drawFastHLine(x0, mid, w, kDim);

  for (uint8_t step = 0; step < kStepCount; ++step) {
    int x = x0 + (step * w) / kStepCount;
    uint16_t markColor = (step == seq.currentStep() && seq.playing()) ? kMotion : ((step % 4) == 0 ? kPanel : TFT_BLACK);
    if (markColor != TFT_BLACK) {
      M5Cardputer.Display.drawFastVLine(x, y0, h, markColor);
    }
  }

  int lastX = x0;
  int lastY = mid;
  bool drew = false;
  for (int x = 0; x < w; ++x) {
    int32_t mix = 0;
    uint8_t voices = 0;
    for (uint8_t i = 0; i < audio.visualVoiceCount(); ++i) {
      const AudioEngine::VisualVoice& voice = audio.visualVoice(i);
      if (!voice.active || !voice.pcm || voice.sampleRate == 0) continue;
      uint64_t elapsedUs = nowUs > voice.startedUs ? nowUs - voice.startedUs : 0;
      uint64_t baseFrame = (elapsedUs * voice.sampleRate) / 1000000ULL;
      if (baseFrame >= voice.frameCount) continue;
      uint32_t frame = static_cast<uint32_t>(baseFrame + (static_cast<uint64_t>(x) * 4200ULL) / w);
      if (frame >= voice.frameCount) continue;
      mix += (static_cast<int32_t>(voice.pcm[frame]) * voice.gain) / 255;
      ++voices;
    }
    if (voices == 0) continue;
    mix /= voices;
    int y = mid - constrain(mix / 1700, -28, 28);
    uint16_t lineColor = voices > 1 ? kHit : kAccent;
    if (drew) {
      M5Cardputer.Display.drawLine(lastX, lastY, x0 + x, y, lineColor);
    } else {
      M5Cardputer.Display.drawPixel(x0 + x, y, lineColor);
      drew = true;
    }
    lastX = x0 + x;
    lastY = y;
  }

  if (!drew) {
    M5Cardputer.Display.setTextColor(kDim, kBg);
    M5Cardputer.Display.setCursor(82, mid - 3);
    M5Cardputer.Display.print("trigger pads");
  }

  uint8_t live = liveVisualCount(audio);
  M5Cardputer.Display.setTextColor(seq.recording() ? kRecord : kMuted, kBg);
  M5Cardputer.Display.setCursor(6, 113);
  M5Cardputer.Display.printf("live %u  trig %lu", live, static_cast<unsigned long>(audio.triggerCount()));
  drawMeter(126, 112, 54, 8, live, audio.polyphony(), live ? kHit : kDim);
  if (seq.playing()) {
    int pulse = 188 + ((seq.currentStep() % 4) * 10);
    M5Cardputer.Display.fillRect(pulse, 112, 7, 8, kMotion);
  }
}
