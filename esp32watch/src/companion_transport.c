#include "companion_transport.h"
#include "app_config.h"
#include "main.h"
#include "uart_console.h"
#include "platform_api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef __enable_irq
#define __enable_irq() do {} while (0)
#endif
#ifndef __disable_irq
#define __disable_irq() do {} while (0)
#endif

#if APP_FEATURE_COMPANION_UART
#define COMPANION_UART_RX_QUEUE_CAPACITY 128U
#define COMPANION_UART_TX_QUEUE_CAPACITY 256U
#define COMPANION_UART_TX_CHUNK_CAPACITY 48U

static uint8_t g_companion_uart_rx_byte;
static bool g_companion_transport_ready;
static volatile uint16_t g_companion_rx_head;
static volatile uint16_t g_companion_rx_tail;
static volatile bool g_companion_rx_drop_until_newline;
static volatile bool g_companion_rx_overflow_report_pending;
static uint8_t g_companion_rx_queue[COMPANION_UART_RX_QUEUE_CAPACITY];

static uint8_t g_companion_tx_queue[COMPANION_UART_TX_QUEUE_CAPACITY];
static uint16_t g_companion_tx_head;
static uint16_t g_companion_tx_tail;
static uint16_t g_companion_tx_count;
static uint8_t g_companion_tx_chunk[COMPANION_UART_TX_CHUNK_CAPACITY];
static uint16_t g_companion_tx_chunk_len;
static volatile bool g_companion_tx_in_flight;
static volatile bool g_companion_tx_error_pending;
static bool g_companion_tx_overflow_report_pending;

static void companion_transport_reset_rx_queue(void)
{
    __disable_irq();
    g_companion_rx_head = 0U;
    g_companion_rx_tail = 0U;
    g_companion_rx_drop_until_newline = false;
    g_companion_rx_overflow_report_pending = false;
    __enable_irq();
}

static void companion_transport_reset_tx_queue(void)
{
    g_companion_tx_head = 0U;
    g_companion_tx_tail = 0U;
    g_companion_tx_count = 0U;
    g_companion_tx_chunk_len = 0U;
    g_companion_tx_in_flight = false;
    g_companion_tx_error_pending = false;
    g_companion_tx_overflow_report_pending = false;
    memset(g_companion_tx_queue, 0, sizeof(g_companion_tx_queue));
    memset(g_companion_tx_chunk, 0, sizeof(g_companion_tx_chunk));
}

static bool companion_transport_arm_rx(void)
{
    if (platform_uart_receive_async(&COMPANION_UART_HANDLE, &g_companion_uart_rx_byte, 1U) != PLATFORM_STATUS_OK) {
        g_companion_transport_ready = false;
        return false;
    }
    g_companion_transport_ready = true;
    return true;
}

static bool companion_transport_queue_push_byte(uint8_t byte)
{
    uint16_t next_head = (uint16_t)((g_companion_rx_head + 1U) % COMPANION_UART_RX_QUEUE_CAPACITY);

    if (next_head == g_companion_rx_tail) {
        return false;
    }

    g_companion_rx_queue[g_companion_rx_head] = byte;
    g_companion_rx_head = next_head;
    return true;
}

static bool companion_transport_queue_pop_byte(uint8_t *out)
{
    bool ok = false;

    if (out == NULL) {
        return false;
    }

    __disable_irq();
    if (g_companion_rx_head != g_companion_rx_tail) {
        *out = g_companion_rx_queue[g_companion_rx_tail];
        g_companion_rx_tail = (uint16_t)((g_companion_rx_tail + 1U) % COMPANION_UART_RX_QUEUE_CAPACITY);
        ok = true;
    }
    __enable_irq();
    return ok;
}

static uint16_t companion_transport_tx_free_space(void)
{
    return (uint16_t)(COMPANION_UART_TX_QUEUE_CAPACITY - g_companion_tx_count);
}

static bool companion_transport_tx_push_bytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == NULL || len == 0U) {
        return true;
    }
    if (len > companion_transport_tx_free_space()) {
        return false;
    }

    for (i = 0U; i < len; ++i) {
        g_companion_tx_queue[g_companion_tx_head] = data[i];
        g_companion_tx_head = (uint16_t)((g_companion_tx_head + 1U) % COMPANION_UART_TX_QUEUE_CAPACITY);
        g_companion_tx_count++;
    }
    return true;
}

static uint16_t companion_transport_tx_pop_chunk(uint8_t *out, uint16_t capacity)
{
    uint16_t count = 0U;

    if (out == NULL || capacity == 0U) {
        return 0U;
    }

    while (count < capacity && g_companion_tx_count > 0U) {
        out[count++] = g_companion_tx_queue[g_companion_tx_tail];
        g_companion_tx_tail = (uint16_t)((g_companion_tx_tail + 1U) % COMPANION_UART_TX_QUEUE_CAPACITY);
        g_companion_tx_count--;
    }
    return count;
}

static void companion_transport_mark_tx_overflow(void)
{
    g_companion_tx_overflow_report_pending = true;
}

static bool companion_transport_queue_line(const char *line)
{
    static const uint8_t suffix[] = {'\r', '\n'};
    size_t len;

    if (line == NULL) {
        return false;
    }

    len = strlen(line);
    if (len > (size_t)UINT16_MAX) {
        return false;
    }
    if (((uint16_t)len + (uint16_t)sizeof(suffix)) > companion_transport_tx_free_space()) {
        return false;
    }
    if (!companion_transport_tx_push_bytes((const uint8_t *)line, (uint16_t)len)) {
        return false;
    }
    if (!companion_transport_tx_push_bytes(suffix, (uint16_t)sizeof(suffix))) {
        return false;
    }
    return true;
}

static void companion_transport_service_tx(void)
{
    PlatformStatus tx_status;

    if (!g_companion_transport_ready || g_companion_tx_in_flight) {
        return;
    }
    if (g_companion_tx_count == 0U) {
        return;
    }

    g_companion_tx_chunk_len = companion_transport_tx_pop_chunk(g_companion_tx_chunk, COMPANION_UART_TX_CHUNK_CAPACITY);
    if (g_companion_tx_chunk_len == 0U) {
        return;
    }

    tx_status = platform_uart_transmit_async(&COMPANION_UART_HANDLE, g_companion_tx_chunk, g_companion_tx_chunk_len);
    if (tx_status != PLATFORM_STATUS_OK) {
        g_companion_tx_error_pending = true;
        g_companion_transport_ready = false;
        g_companion_tx_chunk_len = 0U;
        return;
    }
    g_companion_tx_in_flight = true;
}

static void companion_transport_emit_pending_reports(void)
{
    if (g_companion_tx_error_pending && !g_companion_tx_in_flight) {
        g_companion_tx_error_pending = false;
        g_companion_transport_ready = companion_transport_arm_rx();
        if (!companion_transport_queue_line("ERR uart")) {
            companion_transport_mark_tx_overflow();
        }
    }

    if (g_companion_tx_overflow_report_pending && !g_companion_tx_in_flight) {
        g_companion_tx_overflow_report_pending = false;
        companion_transport_reset_tx_queue();
        (void)companion_transport_queue_line("ERR txoverflow");
    }
}

static void companion_transport_write(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;

    if (data == NULL || len == 0U) {
        return;
    }
    if (len > UINT16_MAX) {
        companion_transport_mark_tx_overflow();
        return;
    }
    if (!companion_transport_tx_push_bytes(data, (uint16_t)len)) {
        companion_transport_mark_tx_overflow();
    }
}

bool companion_transport_init(void)
{
    uart_console_init(companion_transport_write, NULL);
    companion_transport_reset_rx_queue();
    companion_transport_reset_tx_queue();
    return companion_transport_arm_rx();
}

void companion_transport_poll(void)
{
    uint8_t byte;
    bool report_overflow;

    report_overflow = false;
    __disable_irq();
    if (g_companion_rx_overflow_report_pending) {
        g_companion_rx_overflow_report_pending = false;
        report_overflow = true;
    }
    __enable_irq();

    if (report_overflow) {
        uart_console_reset();
        if (!companion_transport_queue_line("ERR overflow")) {
            companion_transport_mark_tx_overflow();
        }
    }

    while (companion_transport_queue_pop_byte(&byte)) {
        uart_console_feed(&byte, 1U);
    }

    companion_transport_emit_pending_reports();
    companion_transport_service_tx();
    companion_transport_emit_pending_reports();
    companion_transport_service_tx();
}

void companion_transport_uart_irq_handler(void)
{
    platform_uart_irq_handler(&COMPANION_UART_HANDLE);
}

bool companion_transport_is_ready(void)
{
    return g_companion_transport_ready;
}

void HAL_UART_RxCpltCallback(PlatformUartPort *huart)
{
    if (huart == NULL || huart->Instance != COMPANION_UART_INSTANCE) {
        return;
    }

    if (g_companion_rx_drop_until_newline) {
        if (g_companion_uart_rx_byte == (uint8_t)'\n') {
            g_companion_rx_drop_until_newline = false;
            g_companion_rx_overflow_report_pending = true;
        }
    } else if (!companion_transport_queue_push_byte(g_companion_uart_rx_byte)) {
        g_companion_rx_drop_until_newline = true;
    }

    (void)companion_transport_arm_rx();
}

void HAL_UART_TxCpltCallback(PlatformUartPort *huart)
{
    if (huart == NULL || huart->Instance != COMPANION_UART_INSTANCE) {
        return;
    }

    g_companion_tx_in_flight = false;
    g_companion_tx_chunk_len = 0U;
}

void HAL_UART_ErrorCallback(PlatformUartPort *huart)
{
    if (huart == NULL || huart->Instance != COMPANION_UART_INSTANCE) {
        return;
    }

    uart_console_reset();
    companion_transport_reset_rx_queue();
    companion_transport_reset_tx_queue();
    g_companion_tx_error_pending = true;
    g_companion_transport_ready = false;
    (void)companion_transport_arm_rx();
}
#else
bool companion_transport_init(void)
{
    return false;
}

void companion_transport_poll(void)
{
}

void companion_transport_uart_irq_handler(void)
{
}

bool companion_transport_is_ready(void)
{
    return false;
}
#endif





