#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string>
#include <string.h>
#include <math.h>
#include "esp_system.h"

class String {
public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}
  size_t length() const { return value_.length(); }
  void reserve(size_t n) { value_.reserve(n); }
  const char* c_str() const { return value_.c_str(); }
  String& operator+=(const char* rhs) { value_ += (rhs ? rhs : ""); return *this; }
  String& operator+=(char rhs) { value_ += rhs; return *this; }
  String& operator+=(unsigned long rhs) { value_ += std::to_string(rhs); return *this; }
  String& operator+=(unsigned int rhs) { value_ += std::to_string(rhs); return *this; }
  String& operator+=(int rhs) { value_ += std::to_string(rhs); return *this; }
  String& operator+=(long rhs) { value_ += std::to_string(rhs); return *this; }
  String& operator+=(float rhs) { value_ += std::to_string(rhs); return *this; }
  String& operator+=(double rhs) { value_ += std::to_string(rhs); return *this; }
  String& operator+=(const String& rhs) { value_ += rhs.value_; return *this; }
  int indexOf(const char* needle, int from=0) const {
    if (!needle) return -1;
    const auto pos = value_.find(needle, static_cast<size_t>(from < 0 ? 0 : from));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  int indexOf(char needle, int from=0) const {
    const auto pos = value_.find(needle, static_cast<size_t>(from < 0 ? 0 : from));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  String substring(int start, int end) const {
    const size_t begin = static_cast<size_t>(start < 0 ? 0 : start);
    const size_t finish = static_cast<size_t>(end < 0 ? 0 : end);
    if (begin >= value_.size() || finish <= begin) return String();
    return String(value_.substr(begin, finish - begin));
  }
  char operator[](int idx) const { return (idx >= 0 && static_cast<size_t>(idx) < value_.size()) ? value_[static_cast<size_t>(idx)] : '\0'; }
  long toInt() const { try { return std::stol(value_); } catch (...) { return 0; } }
  void clear() { value_.clear(); }
  std::string str() const { return value_; }
private:
  std::string value_;
};

inline unsigned long host_stub_millis_value = 0;
inline unsigned long host_stub_micros_value = 0;
inline int host_stub_last_pin_mode_pin = -1;
inline int host_stub_last_pin_mode_value = -1;
inline int host_stub_last_digital_write_pin = -1;
inline int host_stub_last_digital_write_value = -1;
inline int host_stub_last_digital_read_pin = -1;
inline int host_stub_digital_read_value = 1;
inline int host_stub_last_analog_read_pin = -1;
inline int host_stub_analog_read_value = 2048;

inline void host_stub_reset_arduino_io() {
  host_stub_millis_value = 0;
  host_stub_micros_value = 0;
  host_stub_last_pin_mode_pin = -1;
  host_stub_last_pin_mode_value = -1;
  host_stub_last_digital_write_pin = -1;
  host_stub_last_digital_write_value = -1;
  host_stub_last_digital_read_pin = -1;
  host_stub_digital_read_value = 1;
  host_stub_last_analog_read_pin = -1;
  host_stub_analog_read_value = 2048;
}

class SerialStub {
public:
  void begin(unsigned long) {}
  void println(const char*) {}
  void printf(const char*, ...) {}
  size_t write(const uint8_t*, size_t size) { return size; }
};
static SerialStub Serial;

inline void delay(unsigned long) {}
inline unsigned long millis() { return host_stub_millis_value; }
inline unsigned long micros() { return host_stub_micros_value; }
struct tm;
inline void configTzTime(const char*, const char*, const char*, const char*) {}
inline bool getLocalTime(tm*, unsigned long=0) { return false; }

constexpr int INPUT = 0;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLUP = 2;
constexpr int OUTPUT_OPEN_DRAIN = 3;
constexpr int HIGH = 1;
constexpr int LOW = 0;

inline void pinMode(int pin, int mode) {
  host_stub_last_pin_mode_pin = pin;
  host_stub_last_pin_mode_value = mode;
}
inline void digitalWrite(int pin, int value) {
  host_stub_last_digital_write_pin = pin;
  host_stub_last_digital_write_value = value;
}
inline int digitalRead(int pin) {
  host_stub_last_digital_read_pin = pin;
  return host_stub_digital_read_value;
}
inline int analogRead(int pin) {
  host_stub_last_analog_read_pin = pin;
  return host_stub_analog_read_value;
}
