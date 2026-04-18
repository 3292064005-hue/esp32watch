#pragma once
#include <stddef.h>
#include <stdint.h>

inline int host_stub_wire_last_begin_sda[2] = {-1, -1};
inline int host_stub_wire_last_begin_scl[2] = {-1, -1};
inline unsigned long host_stub_wire_last_clock_hz[2] = {0UL, 0UL};
inline uint8_t host_stub_wire_available_value[2] = {0U, 0U};
inline uint8_t host_stub_wire_read_value[2] = {0U, 0U};
inline uint8_t host_stub_wire_end_transmission_value[2] = {0U, 0U};
inline size_t host_stub_wire_request_from_value[2] = {0U, 0U};

inline void host_stub_reset_wire() {
  for (int i = 0; i < 2; ++i) {
    host_stub_wire_last_begin_sda[i] = -1;
    host_stub_wire_last_begin_scl[i] = -1;
    host_stub_wire_last_clock_hz[i] = 0UL;
    host_stub_wire_available_value[i] = 0U;
    host_stub_wire_read_value[i] = 0U;
    host_stub_wire_end_transmission_value[i] = 0U;
    host_stub_wire_request_from_value[i] = 0U;
  }
}

class TwoWire {
public:
  explicit TwoWire(int bus = 0) : bus_id(bus) {}
  void begin() {
    host_stub_wire_last_begin_sda[bus_id] = -2;
    host_stub_wire_last_begin_scl[bus_id] = -2;
  }
  void begin(int sda, int scl) {
    host_stub_wire_last_begin_sda[bus_id] = sda;
    host_stub_wire_last_begin_scl[bus_id] = scl;
  }
  void setClock(unsigned long hz) { host_stub_wire_last_clock_hz[bus_id] = hz; }
  void beginTransmission(uint8_t) {}
  size_t write(const uint8_t*, size_t n) { return n; }
  size_t write(uint8_t) { return 1U; }
  uint8_t endTransmission(bool = true) { return host_stub_wire_end_transmission_value[bus_id]; }
  size_t requestFrom(int, int size) { return host_stub_wire_request_from_value[bus_id] ? host_stub_wire_request_from_value[bus_id] : (size_t)size; }
  int available() { return host_stub_wire_available_value[bus_id] ? host_stub_wire_available_value[bus_id] : 1; }
  int read() { return host_stub_wire_read_value[bus_id]; }
private:
  int bus_id;
};

static TwoWire Wire(0);
