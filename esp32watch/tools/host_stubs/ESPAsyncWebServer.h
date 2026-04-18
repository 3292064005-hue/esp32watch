#pragma once
#include "Arduino.h"
#include <stddef.h>
#include <stdint.h>
class AsyncWebParameter { public: String value() const { return String(); } };
class AsyncWebHeader { public: String value() const { return String(); } };
class AsyncWebServerResponse { public: void addHeader(const char*, const char*) {} };
class AsyncWebServerRequest {
public:
  void* _tempObject = nullptr;
  bool hasHeader(const char*) const { return false; }
  AsyncWebHeader* getHeader(const char*) const { return nullptr; }
  bool hasParam(const char*, bool=false) const { return false; }
  AsyncWebParameter* getParam(const char*, bool=false) const { return nullptr; }
  String arg(const char*) const { return String(); }
  AsyncWebServerResponse* beginResponse(int, const char*, const String&) { static AsyncWebServerResponse r; return &r; }
  AsyncWebServerResponse* beginResponse(int, const char*, const char*) { static AsyncWebServerResponse r; return &r; }
  template<typename FSLike>
  AsyncWebServerResponse* beginResponse(FSLike&, const char*, const char*) { static AsyncWebServerResponse r; return &r; }
  void send(AsyncWebServerResponse*) {}
  void send(int, const char*, const String&) {}
  void send(int, const char*, const char*) {}
};
class AsyncWebServer {
public:
  explicit AsyncWebServer(int port = 80) { (void)port; }
  template<typename... Args>
  void on(const char*, int, Args...) {}
  template<typename Handler>
  void onNotFound(Handler) {}
  void begin() {}
};
#define HTTP_GET 0
#define HTTP_POST 1
