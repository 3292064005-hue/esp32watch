#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "Wire.h"

#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_WHITE 1
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF

inline bool host_stub_display_begin_result = true;
inline bool host_stub_display_begin_called = false;
inline uint8_t host_stub_display_last_command = 0;
inline bool host_stub_display_display_called = false;

inline void host_stub_reset_display() {
  host_stub_display_begin_result = true;
  host_stub_display_begin_called = false;
  host_stub_display_last_command = 0;
  host_stub_display_display_called = false;
}

class Adafruit_SSD1306 {
public:
  Adafruit_SSD1306(uint16_t w, uint16_t h, TwoWire*, int16_t) : width(w), height(h) {
    buffer_size = (size_t)w * (size_t)h / 8U;
    buffer = new uint8_t[buffer_size];
    memset(buffer, 0, buffer_size);
  }
  ~Adafruit_SSD1306() { delete[] buffer; }
  bool begin(uint8_t, uint8_t, bool, bool) { host_stub_display_begin_called = true; return host_stub_display_begin_result; }
  void clearDisplay() { memset(buffer, 0, buffer_size); }
  void display() { host_stub_display_display_called = true; }
  void setTextWrap(bool) {}
  void setTextColor(uint16_t) {}
  void setRotation(uint8_t) {}
  void ssd1306_command(uint8_t cmd) { host_stub_display_last_command = cmd; }
  uint8_t* getBuffer() { return buffer; }
private:
  uint16_t width;
  uint16_t height;
  size_t buffer_size;
  uint8_t* buffer;
};
