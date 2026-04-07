#include "companion_proto_internal.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool companion_proto_token_eq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

void companion_proto_skip_spaces(const char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t') {
        ++(*cursor);
    }
}

bool companion_proto_next_token(const char **cursor, char *out, size_t out_size)
{
    size_t n = 0U;
    const char *p = *cursor;

    companion_proto_skip_spaces(&p);
    if (*p == '\0') {
        return false;
    }
    while (*p != '\0' && *p != ' ' && *p != '\t' && n + 1U < out_size) {
        out[n++] = *p++;
    }
    out[n] = '\0';
    while (*p != '\0' && *p != ' ' && *p != '\t') {
        ++p;
    }
    *cursor = p;
    return n > 0U;
}

bool companion_proto_parse_u32_token(const char *cursor, uint32_t *value_out)
{
    char *end = NULL;
    unsigned long long v;

    if (cursor == NULL || value_out == NULL) {
        return false;
    }
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    if (*cursor == '\0' || *cursor == '-') {
        return false;
    }
    errno = 0;
    v = strtoull(cursor, &end, 10);
    if (errno == ERANGE || v > UINT32_MAX) {
        return false;
    }
    if (end == cursor || (*end != '\0' && *end != ' ' && *end != '\t')) {
        return false;
    }
    *value_out = (uint32_t)v;
    return true;
}

static CompanionProtoCommand companion_proto_parse_command_token(const char *token)
{
    if (companion_proto_token_eq(token, "INFO")) return COMPANION_PROTO_CMD_INFO;
    if (companion_proto_token_eq(token, "GET")) return COMPANION_PROTO_CMD_GET;
    if (companion_proto_token_eq(token, "SET")) return COMPANION_PROTO_CMD_SET;
    if (companion_proto_token_eq(token, "SAFECLR")) return COMPANION_PROTO_CMD_SAFECLR;
    if (companion_proto_token_eq(token, "SENSORREINIT")) return COMPANION_PROTO_CMD_SENSORREINIT;
    if (companion_proto_token_eq(token, "EXPORT")) return COMPANION_PROTO_CMD_EXPORT;
    return COMPANION_PROTO_CMD_UNKNOWN;
}

CompanionProtoParseStatus companion_proto_parse_request(const char *request, CompanionParsedRequest *out)
{
    const char *cursor = request;
    char cmd[COMPANION_PROTO_TOKEN_SIZE];

    if (out == NULL) {
        return COMPANION_PROTO_PARSE_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (request == NULL) {
        return COMPANION_PROTO_PARSE_NULL;
    }
    if (!companion_proto_next_token(&cursor, cmd, sizeof(cmd))) {
        return COMPANION_PROTO_PARSE_EMPTY;
    }

    out->command = companion_proto_parse_command_token(cmd);
    if (out->command == COMPANION_PROTO_CMD_GET || out->command == COMPANION_PROTO_CMD_EXPORT) {
        out->has_subject = companion_proto_next_token(&cursor, out->subject, sizeof(out->subject));
        if (!out->has_subject) {
            return COMPANION_PROTO_PARSE_MISSING_SUBJECT;
        }
    } else if (out->command == COMPANION_PROTO_CMD_SET) {
        out->has_key = companion_proto_next_token(&cursor, out->key, sizeof(out->key));
        if (!out->has_key) {
            return COMPANION_PROTO_PARSE_MISSING_KEY;
        }
        if (!companion_proto_parse_u32_token(cursor, &out->value)) {
            return COMPANION_PROTO_PARSE_BAD_VALUE;
        }
        out->has_value = true;
    }

    return COMPANION_PROTO_PARSE_OK;
}
