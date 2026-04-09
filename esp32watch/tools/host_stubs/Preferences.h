#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"
class Preferences {
public:
  bool begin(const char*, bool) { return true; }
  void end() {}
  bool clear() { return true; }
  bool isKey(const char*) const { return false; }
  uint16_t getUShort(const char*, uint16_t def=0) const { return def; }
  uint8_t getUChar(const char*, uint8_t def=0) const { return def; }
  float getFloat(const char*, float def=0.0f) const { return def; }
  String getString(const char*, const char* def="") const { return String(def); }
  size_t getBytesLength(const char*) const { return 0; }
  size_t getBytes(const char*, void*, size_t) const { return 0; }
  size_t putUShort(const char*, uint16_t) { return sizeof(uint16_t); }
  size_t putUChar(const char*, uint8_t) { return sizeof(uint8_t); }
  size_t putFloat(const char*, float) { return sizeof(float); }
  size_t putString(const char*, const char*) { return 0; }
  size_t putBytes(const char*, const void*, size_t n) { return n; }
  bool remove(const char*) { return true; }
};
