#ifndef COMPANION_TRANSPORT_H
#define COMPANION_TRANSPORT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UART-backed companion transport and arm RX interrupts.
 *
 * The transport uses an interrupt-driven RX byte queue and a deferred, chunked TX
 * queue so protocol parsing and response serialization stay out of interrupt context.
 *
 * @param None.
 * @return true when the transport is ready to exchange protocol lines; false when
 *         UART RX could not be armed.
 * @throws None.
 */
bool companion_transport_init(void);

/**
 * @brief Drain deferred companion UART bytes and service queued TX work in the main loop.
 *
 * RX bytes accumulated by the interrupt handler are forwarded to the console parser
 * from this function. Any queued responses are transmitted asynchronously in bounded
 * chunks so the main loop never blocks on a full-line UART send.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void companion_transport_poll(void);

/**
 * @brief Dispatch the companion UART interrupt handler.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void companion_transport_uart_irq_handler(void);

/**
 * @brief Report whether the UART-backed companion transport is currently armed.
 *
 * @param None.
 * @return true when the transport is ready for RX/TX operations.
 * @throws None.
 */
bool companion_transport_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif
