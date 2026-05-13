#pragma once

#include "MpcTypes.h"

static constexpr char kPadKeys[kPadCount] = {
    'q', 'w', 'e', 'r',
    'a', 's', 'd', 'f',
    'z', 'x', 'c', 'v',
    '1', '2', '3', '4'};

inline int8_t keyToPad(char key) {
  if (key >= 'A' && key <= 'Z') {
    key = key - 'A' + 'a';
  }
  for (uint8_t i = 0; i < kPadCount; ++i) {
    if (kPadKeys[i] == key) {
      return i;
    }
  }
  return -1;
}

inline const char* defaultPadLabel(uint8_t pad) {
  static constexpr const char* labels[kPadCount] = {
      "KICK", "SN", "CHH", "OHH",
      "CLAP", "TOM", "RIM", "SHAK",
      "PERC", "FX1", "FX2", "BASS",
      "P13", "P14", "P15", "P16"};
  return pad < kPadCount ? labels[pad] : "PAD";
}
