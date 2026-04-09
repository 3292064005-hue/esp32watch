#pragma once
#include "Arduino.h"
class WiFiClientSecure;
class HTTPClient {
public:
  void setConnectTimeout(uint16_t) {}
  void setTimeout(uint16_t) {}
  bool begin(WiFiClientSecure&, const String&) { return true; }
  int GET() { return 200; }
  String getString() const { return String(); }
  void end() {}
};
#define HTTP_CODE_OK 200
