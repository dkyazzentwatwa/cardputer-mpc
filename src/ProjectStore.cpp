#include "ProjectStore.h"

#include <ArduinoJson.h>
#include <SD.h>
#include "SampleLoader.h"
#include "Sequencer.h"

static void copyTextPs(char* dest, size_t destSize, const char* src) {
  if (!dest || destSize == 0) return;
  if (!src) src = "";
  strlcpy(dest, src, destSize);
}

bool ProjectStore::begin(const char* root) {
  copyTextPs(_root, sizeof(_root), root);
  SD.mkdir(_root);
  char path[96];
  snprintf(path, sizeof(path), "%s/projects", _root);
  SD.mkdir(path);
  snprintf(path, sizeof(path), "%s/logs", _root);
  SD.mkdir(path);
  return true;
}

void ProjectStore::makeProject(Project& project, const char* name, const char* kitPath, const Sequencer& seq) const {
  project = Project();
  copyTextPs(project.name, sizeof(project.name), name);
  copyTextPs(project.kitPath, sizeof(project.kitPath), kitPath);
  project.pattern = seq.pattern();
}

bool ProjectStore::save(const char* relativePath, const Project& project, const SampleLoader& samples) {
  char path[144];
  snprintf(path, sizeof(path), "%s/%s", _root, relativePath);
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    snprintf(_lastMessage, sizeof(_lastMessage), "save failed: %s", relativePath);
    return false;
  }

  JsonDocument doc;
  doc["version"] = 1;
  doc["name"] = project.name;
  doc["kit"] = project.kitPath;
  doc["bpm"] = project.pattern.bpm;
  JsonObject pattern = doc["pattern"].to<JsonObject>();
  JsonArray steps = pattern["steps"].to<JsonArray>();

  for (uint8_t step = 0; step < project.pattern.stepCount; ++step) {
    JsonObject stepObj = steps.add<JsonObject>();
    JsonArray trig = stepObj["trig"].to<JsonArray>();
    JsonArray vel = stepObj["vel"].to<JsonArray>();
    for (uint8_t pad = 0; pad < kPadCount; ++pad) {
      if (project.pattern.steps[step].triggerMask & (1U << pad)) {
        trig.add(samples.pad(pad).label);
        vel.add(project.pattern.steps[step].velocity[pad]);
      }
    }
  }

  serializeJsonPretty(doc, file);
  file.close();
  snprintf(_lastMessage, sizeof(_lastMessage), "saved %s", relativePath);
  return true;
}

bool ProjectStore::load(const char* relativePath, Project& project, const SampleLoader& samples) {
  char path[144];
  snprintf(path, sizeof(path), "%s/%s", _root, relativePath);
  File file = SD.open(path, FILE_READ);
  if (!file) {
    snprintf(_lastMessage, sizeof(_lastMessage), "load missing: %s", relativePath);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    snprintf(_lastMessage, sizeof(_lastMessage), "project json: %s", err.c_str());
    return false;
  }

  project = Project();
  copyTextPs(project.name, sizeof(project.name), doc["name"] | "loaded");
  copyTextPs(project.kitPath, sizeof(project.kitPath), doc["kit"] | "kits/starter.json");
  project.pattern.bpm = doc["bpm"] | 92;
  project.pattern.stepCount = kStepCount;

  JsonArray steps = doc["pattern"]["steps"].as<JsonArray>();
  uint8_t stepIndex = 0;
  for (JsonObject stepObj : steps) {
    if (stepIndex >= kStepCount) break;
    JsonArray trig = stepObj["trig"].as<JsonArray>();
    JsonArray vel = stepObj["vel"].as<JsonArray>();
    uint8_t eventIndex = 0;
    for (JsonVariant labelVar : trig) {
      const char* label = labelVar.as<const char*>();
      uint8_t pad = samples.padForLabel(label);
      if (pad < kPadCount) {
        uint8_t velocity = vel[eventIndex] | 110;
        project.pattern.steps[stepIndex].triggerMask |= 1U << pad;
        project.pattern.steps[stepIndex].velocity[pad] = velocity;
      }
      ++eventIndex;
    }
    ++stepIndex;
  }

  snprintf(_lastMessage, sizeof(_lastMessage), "loaded %s", relativePath);
  return true;
}
