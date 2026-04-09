#pragma once
#include "Arduino.h"
#include <stddef.h>
#include <stdint.h>
class AsyncWebParameter { public: String value() const { return String(); } };
class AsyncWebHeader { public: String value() const { return String(); } };
class AsyncWebServerRequest {
public:
  void* _tempObject = nullptr;
  bool hasHeader(const char*) const { return false; }
  AsyncWebHeader* getHeader(const char*) const { return nullptr; }
  bool hasParam(const char*, bool=false) const { return false; }
  AsyncWebParameter* getParam(const char*, bool=false) const { return nullptr; }
  String arg(const char*) const { return String(); }
  void send(int, const char*, const String&) {}
  void send(int, const char*, const char*) {}
};
class AsyncWebServer {
public:
  template<typename... Args>
  void on(const char*, int, Args...) {}
};
#define HTTP_GET 0
#define HTTP_POST 1
