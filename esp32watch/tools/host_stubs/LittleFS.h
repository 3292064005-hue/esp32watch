#pragma once
#include <stddef.h>
#include "Arduino.h"

class File {
public:
  File() = default;
  explicit File(bool valid) : valid_(valid) {}
  explicit operator bool() const { return valid_; }
  int available() const { return 0; }
  int read() { return -1; }
  void close() {}
private:
  bool valid_ = true;
};

class LittleFSClass {
public:
  bool begin(bool formatOnFail = false,
             const char* basePath = "/littlefs",
             unsigned char maxOpenFiles = 10,
             const char* partitionLabel = "littlefs") {
    (void)formatOnFail;
    (void)basePath;
    (void)maxOpenFiles;
    (void)partitionLabel;
    return true;
  }
  bool format() { return true; }
  bool exists(const char*) const { return true; }
  File open(const char*, const char* = "r") const { return File(true); }
};
static LittleFSClass LittleFS;
