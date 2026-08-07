#ifndef USART1_H
#define USART1_H

#include <stdint.h>

/**
 * @brief  Configures USART1 for interrupt-driven, full-duplex operation
 *         at 115200 baud (PA9 = TX, PA10 = RX).
 * @note   Must be called after SystemInit() and after SystemCoreClock
 *         reflects the real core clock, since the baud rate divisor is
 *         calculated from PCLK2 (72MHz, assuming default APB2 prescaler).
 * @retval None
 */
void USART1_Init(void);

/**
 * @brief  Reads one byte from the RX ring buffer, if available.
 * @note   Non-blocking. Safe to call from any context.
 * @retval The next received byte (0-255), or -1 if the buffer is empty.
 */
int usart_read_byte(void);

/**
 * @brief  Queues a null-terminated string for transmission.
 * @note   Non-blocking: copies the string into the TX ring buffer and
 *         (if idle) kicks off interrupt-driven transmission; the ISR
 *         drains the rest. If the string is longer than the currently
 *         available buffer space, the entire call is rejected and
 *         nothing is sent - no partial writes.
 * @param  str Null-terminated string to send.
 * @retval None
 */
void usart_send_string(const char *str);

/**
 * @brief  Returns the number of received bytes that were dropped
 *         because the RX buffer was full when they arrived.
 * @note   Useful as a diagnostic - a nonzero/growing count means the
 *         application isn't draining usart_read_byte() fast enough
 *         for the incoming data rate.
 * @retval Total dropped-byte count since startup.
 */
uint32_t usart_get_rx_overflow_count(void);

#endif
