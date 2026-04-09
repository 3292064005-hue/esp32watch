#pragma once
#include "Arduino.h"
#include <stdint.h>
class IPAddress {
public:
  uint8_t operator[](int) const { return 0; }
};
enum wl_status_t { WL_CONNECTED = 3 };
enum { WIFI_AP = 1, WIFI_STA = 2, WIFI_AP_STA = 3 };
class WiFiClass {
public:
  bool softAP(const char*, const char*, int=1, bool=false, int=4) { return true; }
  void softAPdisconnect(bool) {}
  IPAddress softAPIP() const { return IPAddress(); }
  IPAddress localIP() const { return IPAddress(); }
  void mode(int) {}
  void setAutoConnect(bool) {}
  void setAutoReconnect(bool) {}
  void setSleep(bool) {}
  void persistent(bool) {}
  void disconnect(bool=true, bool=true) {}
  void begin(const char*, const char*) {}
  wl_status_t status() const { return WL_CONNECTED; }
  int32_t RSSI() const { return -50; }
};
static WiFiClass WiFi;
