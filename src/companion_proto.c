#include "companion_proto.h"
#include "companion_proto_internal.h"

size_t companion_proto_process_line(const char *request, char *out, size_t out_size)
{
    CompanionParsedRequest parsed;
    CompanionProtoParseStatus status = companion_proto_parse_request(request, &parsed);

    switch (status) {
        case COMPANION_PROTO_PARSE_NULL:
            return companion_proto_write_response(out, out_size, "ERR null");
        case COMPANION_PROTO_PARSE_EMPTY:
            return companion_proto_write_response(out, out_size, "ERR empty");
        case COMPANION_PROTO_PARSE_MISSING_SUBJECT:
            return companion_proto_write_response(out, out_size, "ERR missing subject");
        case COMPANION_PROTO_PARSE_MISSING_KEY:
            return companion_proto_write_response(out, out_size, "ERR missing key");
        case COMPANION_PROTO_PARSE_BAD_VALUE:
            return companion_proto_write_response(out, out_size, "ERR bad value");
        case COMPANION_PROTO_PARSE_OK:
        default:
            break;
    }

    return companion_proto_handle_request(&parsed, out, out_size);
}
