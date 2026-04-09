#pragma once
#include <stdint.h>
class ESPClass {
public:
  uint64_t getEfuseMac() const { return 0x1122334455667788ULL; }
};
static ESPClass ESP;
