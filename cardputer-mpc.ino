#include <M5Cardputer.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
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

struct KitSlot {
  const char* path;
  const char* label;
};

static constexpr KitSlot kKits[] = {
    {"kits/starter.json", "starter"},
    {"kits/8bit.json", "8bit"},
};
static constexpr uint8_t kKitCount = sizeof(kKits) / sizeof(kKits[0]);
static uint8_t currentKitIndex = 0;
static const char* currentKit = kKits[currentKitIndex].path;

static void returnToCypherOs(uint32_t delayMs = 250) {
  Preferences prefs;
  if (prefs.begin("cyputeros", false)) {
    prefs.putBool("returnOnce", true);
    prefs.putBool("bootToApp", false);
    prefs.end();
  }

  const esp_partition_t* launcher = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0");
  if (!launcher) {
    launcher = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  }
  if (launcher) {
    esp_ota_set_boot_partition(launcher);
  }
  if (delayMs > 0) delay(delayMs);
  ESP.restart();
}

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
  currentKitIndex = 0;
  currentKit = kKits[currentKitIndex].path;
  samples.loadKit(currentKit);
  if (projects.load("projects/demo-groove.json", project, samples)) {
    seq.setPattern(project.pattern);
    ui.setStatus(projects.lastMessage());
  } else {
    ui.setStatus(projects.lastMessage());
  }
}

static void cycleKit() {
  if (!sdReady) {
    ui.setStatus("no SD");
    return;
  }

  audio.panic();
  seq.stop();
  currentKitIndex = (currentKitIndex + 1) % kKitCount;
  currentKit = kKits[currentKitIndex].path;
  bool loaded = samples.loadKit(currentKit);

  char status[32];
  snprintf(status, sizeof(status), "kit %s", kKits[currentKitIndex].label);
  ui.setStatus(loaded ? status : samples.lastMessage());
  ui.force();
}

static void handleKey(char key, bool shift) {
  if ((key == 'Q' || key == 'q') && shift) {
    audio.panic();
    seq.stop();
    ui.setStatus("returning to Cypher OS");
    ui.force();
    returnToCypherOs();
    return;
  }

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
      cycleKit();
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
