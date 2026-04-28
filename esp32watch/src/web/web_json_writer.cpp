#include "web/web_json_writer.h"

WebJsonWriter::WebJsonWriter(String &out) :
    out_(out),
    depth_(0U),
    ok_(true),
    overflowed_(false)
{
}

bool WebJsonWriter::ok() const
{
    return ok_ && !overflowed_;
}

bool WebJsonWriter::overflowed() const
{
    return overflowed_;
}

void WebJsonWriter::markError()
{
    ok_ = false;
}

bool WebJsonWriter::pushScope()
{
    if (depth_ >= kMaxDepth) {
        overflowed_ = true;
        ok_ = false;
        return false;
    }
    scopes_[depth_].first = true;
    scopes_[depth_].expecting_value = false;
    ++depth_;
    return true;
}

void WebJsonWriter::beforeValue()
{
    if (depth_ == 0U) {
        return;
    }
    Scope &scope = scopes_[depth_ - 1U];
    if (scope.expecting_value) {
        scope.expecting_value = false;
        return;
    }
    if (!scope.first) {
        out_ += ',';
    }
    scope.first = false;
}

void WebJsonWriter::writeKey(const char *key_name)
{
    if (depth_ == 0U) {
        markError();
        return;
    }
    Scope &scope = scopes_[depth_ - 1U];
    if (scope.expecting_value) {
        markError();
        return;
    }
    if (!scope.first) {
        out_ += ',';
    }
    scope.first = false;
    out_ += '"';
    web_json_escape_append(out_, key_name != nullptr ? key_name : "");
    out_ += "\":";
    scope.expecting_value = true;
}

void WebJsonWriter::beginObject()
{
    beforeValue();
    out_ += '{';
    (void)pushScope();
}

void WebJsonWriter::endObject()
{
    if (depth_ == 0U) {
        markError();
        return;
    }
    if (scopes_[depth_ - 1U].expecting_value) {
        markError();
    }
    out_ += '}';
    --depth_;
}

void WebJsonWriter::beginArray(const char *key_name)
{
    writeKey(key_name);
    beforeValue();
    out_ += '[';
    (void)pushScope();
}

void WebJsonWriter::endArray()
{
    if (depth_ == 0U) {
        markError();
        return;
    }
    if (scopes_[depth_ - 1U].expecting_value) {
        markError();
    }
    out_ += ']';
    --depth_;
}

void WebJsonWriter::key(const char *key_name)
{
    writeKey(key_name);
}

void WebJsonWriter::kv(const char *key_name, const char *value)
{
    writeKey(key_name);
    beforeValue();
    out_ += '"';
    web_json_escape_append(out_, value != nullptr ? value : "");
    out_ += '"';
}

void WebJsonWriter::kv(const char *key_name, bool value)
{
    writeKey(key_name);
    beforeValue();
    out_ += value ? "true" : "false";
}

void WebJsonWriter::kv(const char *key_name, uint32_t value)
{
    writeKey(key_name);
    beforeValue();
    out_ += value;
}

void WebJsonWriter::kv(const char *key_name, int32_t value)
{
    writeKey(key_name);
    beforeValue();
    out_ += value;
}
