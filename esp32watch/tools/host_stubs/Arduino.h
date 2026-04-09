#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string>
#include <math.h>
#include "esp_system.h"
class String {
public:
  String() = default;
  String(const char*) {}
  String(const std::string&) {}
  size_t length() const { return 0; }
  void reserve(size_t) {}
  const char* c_str() const { return ""; }
  String& operator+=(const char*) { return *this; }
  String& operator+=(char) { return *this; }
  String& operator+=(unsigned long) { return *this; }
  String& operator+=(unsigned int) { return *this; }
  String& operator+=(int) { return *this; }
  String& operator+=(const String&) { return *this; }
  int indexOf(const char*, int=0) const { return -1; }
  int indexOf(char, int=0) const { return -1; }
  String substring(int, int) const { return String(); }
  char operator[](int) const { return '\0'; }
  long toInt() const { return 0; }
  void clear() {}
};
class SerialStub {
public:
  void begin(unsigned long) {}
  void println(const char*) {}
  void printf(const char*, ...) {}
};
static SerialStub Serial;
inline void delay(unsigned long) {}
inline unsigned long millis() { return 0; }
struct tm;
inline void configTzTime(const char*, const char*, const char*, const char*) {}
inline bool getLocalTime(tm*, unsigned long=0) { return false; }
