#pragma once
#include "Arduino.h"
#include <stdint.h>
#include <string.h>
class IPAddress {
public:
  IPAddress(uint8_t a=0, uint8_t b=0, uint8_t c=0, uint8_t d=0) { bytes[0]=a; bytes[1]=b; bytes[2]=c; bytes[3]=d; }
  uint8_t operator[](int i) const { return bytes[i & 3]; }
private:
  uint8_t bytes[4];
};
enum wl_status_t { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_AP = 1, WIFI_STA = 2, WIFI_AP_STA = 3 };

inline wl_status_t host_stub_wifi_status = WL_CONNECTED;
inline int host_stub_wifi_mode = WIFI_STA;
inline bool host_stub_softap_active = false;
inline char host_stub_last_wifi_ssid[65] = {0};
inline char host_stub_last_wifi_password[65] = {0};
inline IPAddress host_stub_local_ip = IPAddress(192,168,1,10);
inline IPAddress host_stub_ap_ip = IPAddress(192,168,4,1);
inline int32_t host_stub_wifi_rssi = -50;

inline void host_stub_reset_wifi() {
  host_stub_wifi_status = WL_CONNECTED;
  host_stub_wifi_mode = WIFI_STA;
  host_stub_softap_active = false;
  host_stub_last_wifi_ssid[0] = '\0';
  host_stub_last_wifi_password[0] = '\0';
  host_stub_local_ip = IPAddress(192,168,1,10);
  host_stub_ap_ip = IPAddress(192,168,4,1);
  host_stub_wifi_rssi = -50;
}

class WiFiClass {
public:
  bool softAP(const char* ssid, const char* password, int=1, bool=false, int=4) {
    host_stub_softap_active = true;
    strncpy(host_stub_last_wifi_ssid, ssid ? ssid : "", sizeof(host_stub_last_wifi_ssid) - 1U);
    strncpy(host_stub_last_wifi_password, password ? password : "", sizeof(host_stub_last_wifi_password) - 1U);
    return true;
  }
  void softAPdisconnect(bool) { host_stub_softap_active = false; }
  IPAddress softAPIP() const { return host_stub_ap_ip; }
  IPAddress localIP() const { return host_stub_local_ip; }
  void mode(int mode_value) { host_stub_wifi_mode = mode_value; }
  void setAutoConnect(bool) {}
  void setAutoReconnect(bool) {}
  void setSleep(bool) {}
  void persistent(bool) {}
  void disconnect(bool=true, bool=true) {}
  void begin(const char* ssid, const char* password) {
    strncpy(host_stub_last_wifi_ssid, ssid ? ssid : "", sizeof(host_stub_last_wifi_ssid) - 1U);
    strncpy(host_stub_last_wifi_password, password ? password : "", sizeof(host_stub_last_wifi_password) - 1U);
  }
  wl_status_t status() const { return host_stub_wifi_status; }
  int32_t RSSI() const { return host_stub_wifi_rssi; }
};
static WiFiClass WiFi;
