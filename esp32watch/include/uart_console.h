#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*UartConsoleWriteFn)(const uint8_t *data, size_t len, void *ctx);

void uart_console_init(UartConsoleWriteFn write_fn, void *write_ctx);
void uart_console_reset(void);
void uart_console_feed(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
