#pragma once

#include <Arduino.h>
#include "MpcTypes.h"

class SampleLoader;
class Sequencer;

class ProjectStore {
public:
  bool begin(const char* root);
  bool save(const char* relativePath, const Project& project, const SampleLoader& samples);
  bool load(const char* relativePath, Project& project, const SampleLoader& samples);
  void makeProject(Project& project, const char* name, const char* kitPath, const Sequencer& seq) const;
  const char* lastMessage() const { return _lastMessage; }

private:
  char _root[64] = {0};
  char _lastMessage[96] = "project store ready";
};
