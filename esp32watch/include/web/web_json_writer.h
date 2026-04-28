#ifndef WEB_JSON_WRITER_H
#define WEB_JSON_WRITER_H

#include <Arduino.h>
#include "web/web_json.h"

class WebJsonWriter {
public:
    explicit WebJsonWriter(String &out);

    void beginObject();
    void endObject();
    void beginArray(const char *key);
    void endArray();
    void key(const char *key);
    void kv(const char *key, const char *value);
    void kv(const char *key, bool value);
    void kv(const char *key, uint32_t value);
    void kv(const char *key, int32_t value);

    bool ok() const;
    bool overflowed() const;

private:
    struct Scope {
        bool first;
        bool expecting_value;
    };

    static constexpr uint8_t kMaxDepth = 8U;

    bool pushScope();
    void markError();
    void beforeValue();
    void writeKey(const char *key_name);

    String &out_;
    Scope scopes_[kMaxDepth];
    uint8_t depth_;
    bool ok_;
    bool overflowed_;
};

#endif
