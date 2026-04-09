#pragma once
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
};
static LittleFSClass LittleFS;
