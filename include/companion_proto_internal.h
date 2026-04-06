#ifndef COMPANION_PROTO_INTERNAL_H
#define COMPANION_PROTO_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMPANION_PROTO_TOKEN_SIZE 24U
#define COMPANION_PROTO_VERSION 4U

typedef enum {
    COMPANION_PROTO_CMD_NONE = 0,
    COMPANION_PROTO_CMD_INFO,
    COMPANION_PROTO_CMD_GET,
    COMPANION_PROTO_CMD_SET,
    COMPANION_PROTO_CMD_SAFECLR,
    COMPANION_PROTO_CMD_SENSORREINIT,
    COMPANION_PROTO_CMD_EXPORT,
    COMPANION_PROTO_CMD_UNKNOWN
} CompanionProtoCommand;

typedef enum {
    COMPANION_PROTO_PARSE_OK = 0,
    COMPANION_PROTO_PARSE_NULL,
    COMPANION_PROTO_PARSE_EMPTY,
    COMPANION_PROTO_PARSE_MISSING_SUBJECT,
    COMPANION_PROTO_PARSE_MISSING_KEY,
    COMPANION_PROTO_PARSE_BAD_VALUE
} CompanionProtoParseStatus;

typedef struct {
    CompanionProtoCommand command;
    char subject[COMPANION_PROTO_TOKEN_SIZE];
    char key[COMPANION_PROTO_TOKEN_SIZE];
    uint32_t value;
    bool has_subject;
    bool has_key;
    bool has_value;
} CompanionParsedRequest;

bool companion_proto_token_eq(const char *a, const char *b);
void companion_proto_skip_spaces(const char **cursor);
bool companion_proto_next_token(const char **cursor, char *out, size_t out_size);
bool companion_proto_parse_u32_token(const char *cursor, uint32_t *value_out);
CompanionProtoParseStatus companion_proto_parse_request(const char *request, CompanionParsedRequest *out);
size_t companion_proto_write_response(char *out, size_t out_size, const char *fmt, ...);
size_t companion_proto_handle_request(const CompanionParsedRequest *request, char *out, size_t out_size);
size_t companion_proto_format_info(char *out, size_t out_size);
size_t companion_proto_format_settings(char *out, size_t out_size);
size_t companion_proto_format_diag(char *out, size_t out_size);
size_t companion_proto_format_sensor(char *out, size_t out_size);
size_t companion_proto_format_faults(char *out, size_t out_size);
size_t companion_proto_format_activity(char *out, size_t out_size);
size_t companion_proto_format_storage(char *out, size_t out_size);
size_t companion_proto_format_clock(char *out, size_t out_size);
size_t companion_proto_format_perf(char *out, size_t out_size);
size_t companion_proto_format_proto(char *out, size_t out_size);

#endif
