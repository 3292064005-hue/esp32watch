#pragma once
#include "Arduino.h"
class JsonObject {};
class JsonVariant {
public:
  bool isNull() const { return false; }
  template<typename T> bool is() const { return true; }
  JsonVariant operator[](const char*) const { return JsonVariant(); }
  template<typename T> T as() const { return T(); }
};
struct DeserializationError {
  operator bool() const { return false; }
  const char* c_str() const { return ""; }
};
template <size_t N>
class StaticJsonDocument {
public:
  JsonVariant operator[](const char*) const { return JsonVariant(); }
};
template <typename T>
DeserializationError deserializeJson(T&, const String&) { return DeserializationError(); }
