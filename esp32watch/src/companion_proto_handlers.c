#include "companion_proto_internal.h"
#include "app_command.h"
#include "services/diag_service.h"
#include <stdbool.h>
#include <stdint.h>

static size_t companion_proto_handle_get_subject(const char *subject, char *out, size_t out_size)
{
    if (companion_proto_token_eq(subject, "INFO")) return companion_proto_format_info(out, out_size);
    if (companion_proto_token_eq(subject, "SETTINGS")) return companion_proto_format_settings(out, out_size);
    if (companion_proto_token_eq(subject, "DIAG")) return companion_proto_format_diag(out, out_size);
    if (companion_proto_token_eq(subject, "FAULTS")) return companion_proto_format_faults(out, out_size);
    if (companion_proto_token_eq(subject, "SENSOR")) return companion_proto_format_sensor(out, out_size);
    if (companion_proto_token_eq(subject, "ACTIVITY")) return companion_proto_format_activity(out, out_size);
    if (companion_proto_token_eq(subject, "STORAGE")) return companion_proto_format_storage(out, out_size);
    if (companion_proto_token_eq(subject, "CLOCK")) return companion_proto_format_clock(out, out_size);
    if (companion_proto_token_eq(subject, "PERF")) return companion_proto_format_perf(out, out_size);
    if (companion_proto_token_eq(subject, "PROTO")) return companion_proto_format_proto(out, out_size);
    return companion_proto_write_response(out, out_size, "ERR bad subject");
}

static size_t companion_proto_handle_export_subject(const char *subject, char *out, size_t out_size)
{
    if (companion_proto_token_eq(subject, "STORAGE")) return companion_proto_format_storage(out, out_size);
    if (companion_proto_token_eq(subject, "ACTIVITY")) return companion_proto_format_activity(out, out_size);
    if (companion_proto_token_eq(subject, "DIAG")) return companion_proto_format_diag(out, out_size);
    if (companion_proto_token_eq(subject, "CLOCK")) return companion_proto_format_clock(out, out_size);
    if (companion_proto_token_eq(subject, "PERF")) return companion_proto_format_perf(out, out_size);
    if (companion_proto_token_eq(subject, "PROTO")) return companion_proto_format_proto(out, out_size);
    return companion_proto_write_response(out, out_size, "ERR bad subject");
}

/**
 * @brief Map a protocol SET request into the shared application command ingress.
 *
 * @param[in] key Companion protocol key token.
 * @param[in] value Parsed unsigned integer payload.
 * @param[out] out Destination command descriptor populated on success.
 * @return true when the key maps to a supported command; false otherwise.
 * @throws None.
 * @boundary_behavior Returns false when any pointer is NULL or the key is unsupported.
 */
static bool companion_proto_build_set_command(const char *key, uint32_t value, AppCommand *out)
{
    if (key == NULL || out == NULL) {
        return false;
    }

    *out = (AppCommand){
        .source = APP_COMMAND_SOURCE_COMPANION,
        .type = APP_COMMAND_NONE,
    };

    if (companion_proto_token_eq(key, "BRIGHTNESS")) {
        out->type = APP_COMMAND_SET_BRIGHTNESS;
        out->data.u8 = (uint8_t)value;
        return true;
    }
    if (companion_proto_token_eq(key, "GOAL")) {
        out->type = APP_COMMAND_SET_GOAL;
        out->data.u32 = value;
        return true;
    }
    if (companion_proto_token_eq(key, "AUTOWAKE")) {
        out->type = APP_COMMAND_SET_AUTO_WAKE;
        out->data.enabled = value != 0U;
        return true;
    }
    if (companion_proto_token_eq(key, "AUTOSLEEP")) {
        out->type = APP_COMMAND_SET_AUTO_SLEEP;
        out->data.enabled = value != 0U;
        return true;
    }
    if (companion_proto_token_eq(key, "DND")) {
        out->type = APP_COMMAND_SET_DND;
        out->data.enabled = value != 0U;
        return true;
    }
    if (companion_proto_token_eq(key, "VIBRATE")) {
        out->type = APP_COMMAND_SET_VIBRATE;
        out->data.enabled = value != 0U;
        return true;
    }
    if (companion_proto_token_eq(key, "SECONDS")) {
        out->type = APP_COMMAND_SET_SHOW_SECONDS;
        out->data.enabled = value != 0U;
        return true;
    }
    if (companion_proto_token_eq(key, "ANIM")) {
        out->type = APP_COMMAND_SET_ANIMATIONS;
        out->data.enabled = value != 0U;
        return true;
    }
    if (companion_proto_token_eq(key, "FACE")) {
        out->type = APP_COMMAND_SET_WATCHFACE;
        out->data.u8 = (uint8_t)value;
        return true;
    }
    if (companion_proto_token_eq(key, "SENS")) {
        out->type = APP_COMMAND_SET_SENSOR_SENSITIVITY;
        out->data.u8 = (uint8_t)value;
        return true;
    }
    if (companion_proto_token_eq(key, "TIMEOUT")) {
        out->type = APP_COMMAND_SET_SCREEN_TIMEOUT_IDX;
        out->data.u8 = (uint8_t)value;
        return true;
    }

    return false;
}

static size_t companion_proto_handle_set_key(const char *key, uint32_t value, char *out, size_t out_size)
{
    AppCommand command;

    if (!companion_proto_build_set_command(key, value, &command)) {
        return companion_proto_write_response(out, out_size, "ERR bad key");
    }
    if (!app_command_execute(&command, NULL)) {
        return companion_proto_write_response(out, out_size, "ERR blocked");
    }
    return companion_proto_write_response(out, out_size, "OK");
}

size_t companion_proto_handle_request(const CompanionParsedRequest *request, char *out, size_t out_size)
{
    if (request == NULL) {
        return companion_proto_write_response(out, out_size, "ERR null");
    }

    switch (request->command) {
        case COMPANION_PROTO_CMD_INFO:
            return companion_proto_format_info(out, out_size);
        case COMPANION_PROTO_CMD_GET:
            return companion_proto_handle_get_subject(request->subject, out, out_size);
        case COMPANION_PROTO_CMD_SET:
            return companion_proto_handle_set_key(request->key, request->value, out, out_size);
        case COMPANION_PROTO_CMD_SAFECLR: {
            AppCommand command = {.source = APP_COMMAND_SOURCE_COMPANION, .type = APP_COMMAND_CLEAR_SAFE_MODE};
            if (!app_command_execute(&command, NULL)) {
                return companion_proto_write_response(out, out_size, "ERR blocked");
            }
            return companion_proto_write_response(out, out_size, "OK");
        }
        case COMPANION_PROTO_CMD_SENSORREINIT: {
            AppCommand command = {.source = APP_COMMAND_SOURCE_COMPANION, .type = APP_COMMAND_SENSOR_REINIT};
            return companion_proto_write_response(out, out_size, app_command_execute(&command, NULL) ? "OK" : "ERR reinit");
        }
        case COMPANION_PROTO_CMD_EXPORT:
            return companion_proto_handle_export_subject(request->subject, out, out_size);
        case COMPANION_PROTO_CMD_UNKNOWN:
        case COMPANION_PROTO_CMD_NONE:
        default:
            return companion_proto_write_response(out, out_size, "ERR unknown");
    }
}
