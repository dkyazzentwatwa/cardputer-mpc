#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>
#include "src/AudioEngine.h"
#include "src/InputMap.h"
#include "src/ProjectStore.h"
#include "src/SampleLoader.h"
#include "src/Sequencer.h"
#include "src/Ui.h"

static AudioEngine audio;
static SampleLoader samples;
static Sequencer seq;
static ProjectStore projects;
static Ui ui;
static bool sdReady = false;
static const char* currentKit = "kits/starter.json";

static void triggerPad(uint8_t pad, uint8_t velocity) {
  audio.trigger(pad, velocity);
}

static void saveCurrentProject() {
  if (!sdReady) {
    ui.setStatus("no SD");
    return;
  }
  Project project;
  projects.makeProject(project, "cardputer-jam", currentKit, seq);
  if (projects.save("projects/cardputer-jam.json", project, samples)) {
    ui.setStatus(projects.lastMessage());
  } else {
    ui.setStatus(projects.lastMessage());
  }
}

static void loadDemoProject() {
  if (!sdReady) {
    ui.setStatus("no SD");
    return;
  }
  Project project;
  if (projects.load("projects/demo-groove.json", project, samples)) {
    seq.setPattern(project.pattern);
    currentKit = "kits/starter.json";
    ui.setStatus(projects.lastMessage());
  } else {
    ui.setStatus(projects.lastMessage());
  }
}

static void handleKey(char key, bool shift) {
  if ((key == 'S' || key == 's') && shift) {
    saveCurrentProject();
    return;
  }

  int8_t pad = keyToPad(key);
  if (pad >= 0) {
    audio.trigger(pad, 120);
    if (seq.recordHit(pad, 120)) {
      char status[32];
      snprintf(status, sizeof(status), "rec %s step %02u", samples.pad(pad).label, seq.currentStep() + 1);
      ui.setStatus(status);
    }
    if (ui.view() == UiView::Steps && pad == ui.selectedPad()) {
      seq.toggleStep(ui.selectedStep(), pad, 110);
    }
    ui.force();
    return;
  }

  switch (key) {
    case ' ':
      seq.togglePlay(esp_timer_get_time());
      ui.setStatus(seq.playing() ? (seq.hasEvents() ? "play" : "play empty") : "stop");
      break;
    case '\n':
    case '\r':
      if (!seq.playing()) {
        seq.start(esp_timer_get_time());
        seq.setRecording(true);
      } else {
        seq.toggleRecord();
      }
      ui.setStatus(seq.recording() ? "record on" : "record off");
      break;
    case '\t':
      ui.nextView();
      break;
    case '\b':
      if (ui.view() == UiView::Steps) {
        seq.clearStep(ui.selectedStep());
        ui.setStatus("step cleared");
      } else {
        audio.panic();
        ui.setStatus("panic");
      }
      break;
    case '`':
      audio.panic();
      seq.stop();
      ui.setStatus("panic");
      break;
    case '[':
      ui.selectStep(-1);
      break;
    case ']':
      ui.selectStep(1);
      break;
    case '5':
    case '%':
      ui.selectStep(-1);
      break;
    case '6':
    case '^':
      ui.selectStep(1);
      break;
    case ',':
      if (shift) {
        samples.adjustPadVolume(ui.selectedPad(), -10);
        ui.setStatus(samples.lastMessage());
      } else {
        ui.selectPad(-1);
      }
      break;
    case '<':
      samples.adjustPadVolume(ui.selectedPad(), -10);
      ui.setStatus(samples.lastMessage());
      break;
    case '.':
      if (shift) {
        samples.adjustPadVolume(ui.selectedPad(), 10);
        ui.setStatus(samples.lastMessage());
      } else {
        ui.selectPad(1);
      }
      break;
    case '>':
      samples.adjustPadVolume(ui.selectedPad(), 10);
      ui.setStatus(samples.lastMessage());
      break;
    case '-':
      audio.adjustMasterVolume(-10);
      ui.setStatus(audio.lastMessage());
      ui.force();
      break;
    case '_':
      seq.adjustBpm(-5);
      ui.force();
      break;
    case '=':
      audio.adjustMasterVolume(10);
      ui.setStatus(audio.lastMessage());
      ui.force();
      break;
    case '+':
      seq.adjustBpm(5);
      ui.force();
      break;
    case 'l':
    case 'L':
      loadDemoProject();
      break;
    case 'k':
    case 'K':
      seq.stop();
      samples.loadKit(currentKit);
      ui.setStatus(samples.lastMessage());
      break;
    case 'n':
    case 'N':
      seq.clear();
      ui.setStatus("new pattern");
      break;
    default:
      break;
  }
}

static void handleKeyboard() {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
  for (char key : state.word) {
    if (key == ' ') continue;
    handleKey(key, state.shift);
  }
  if (state.space) handleKey(' ', state.shift);
  if (state.enter) handleKey('\n', state.shift);
  if (state.tab) handleKey('\t', state.shift);
  if (state.del) handleKey('\b', state.shift);
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  ui.begin();
  ui.showIntro();
  ui.setStatus("boot");

  SPI.begin(40, 39, 14, 12);
  sdReady = SD.begin(12, SPI, 25000000);
  samples.begin(kSdRoot);

  if (sdReady) {
    SD.mkdir(kSdRoot);
    SD.mkdir("/cardputer-mpc/kits");
    SD.mkdir("/cardputer-mpc/projects");
    SD.mkdir("/cardputer-mpc/samples");
    projects.begin(kSdRoot);
    samples.loadKit(currentKit);
  } else {
    samples.createFallbackKit("SD missing");
    ui.setStatus("SD missing, fallback");
  }

  audio.begin(&samples);
  seq.begin(92);

  Serial.printf("[mpc] boot sd=%d kit=%s heap=%lu largest=%u\n",
                sdReady,
                samples.kitName(),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void loop() {
  M5Cardputer.update();
  uint64_t nowUs = esp_timer_get_time();

  handleKeyboard();
  seq.update(nowUs, triggerPad);
  audio.update();
  ui.update(nowUs, seq, samples, audio);
}
