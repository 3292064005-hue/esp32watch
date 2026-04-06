#include "uart_console.h"
#include "companion_proto.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define UART_CONSOLE_LINE_MAX 96U
#define UART_CONSOLE_RESPONSE_MAX 160U

static struct {
    UartConsoleWriteFn write_fn;
    void *write_ctx;
    char line[UART_CONSOLE_LINE_MAX];
    size_t len;
    bool overflow;
} g_console;

static void uart_console_write_framed(const char *s, size_t len)
{
    char framed[UART_CONSOLE_RESPONSE_MAX + 2U];

    if (g_console.write_fn == NULL || s == NULL || len == 0U) {
        return;
    }
    if (len > UART_CONSOLE_RESPONSE_MAX) {
        len = UART_CONSOLE_RESPONSE_MAX;
    }
    memcpy(framed, s, len);
    framed[len] = '\r';
    framed[len + 1U] = '\n';
    g_console.write_fn((const uint8_t *)framed, len + 2U, g_console.write_ctx);
}

static void uart_console_write_literal(const char *s)
{
    if (s == NULL) {
        return;
    }
    uart_console_write_framed(s, strlen(s));
}

static void uart_console_flush_line(void)
{
    char response[UART_CONSOLE_RESPONSE_MAX];
    size_t n;

    if (g_console.overflow) {
        uart_console_write_literal("ERR overflow");
        g_console.len = 0U;
        g_console.overflow = false;
        return;
    }

    g_console.line[g_console.len] = '\0';
    n = companion_proto_process_line(g_console.line, response, sizeof(response));
    if (n > 0U) {
        uart_console_write_framed(response, n);
    }
    g_console.len = 0U;
}

void uart_console_init(UartConsoleWriteFn write_fn, void *write_ctx)
{
    g_console.write_fn = write_fn;
    g_console.write_ctx = write_ctx;
    g_console.len = 0U;
    g_console.overflow = false;
    memset(g_console.line, 0, sizeof(g_console.line));
}

void uart_console_reset(void)
{
    g_console.len = 0U;
    g_console.overflow = false;
    memset(g_console.line, 0, sizeof(g_console.line));
}

void uart_console_feed(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) {
        return;
    }
    for (size_t i = 0U; i < len; ++i) {
        uint8_t ch = data[i];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (g_console.len > 0U || g_console.overflow) {
                uart_console_flush_line();
            }
            continue;
        }
        if (g_console.overflow) {
            continue;
        }
        if (g_console.len + 1U >= sizeof(g_console.line)) {
            g_console.len = 0U;
            g_console.overflow = true;
            continue;
        }
        g_console.line[g_console.len++] = (char)ch;
    }
}
