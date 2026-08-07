#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/**
 * @brief  Configures and starts the SysTick timer to generate a 1ms
 *         periodic interrupt, using SystemCoreClock as the timing
 *         reference (see SystemInit.c).
 * @note   Must be called once at startup, after SystemInit() has
 *         configured the core clock, and before any code relies on
 *         systick_get_ms() or delay_ms().
 * @retval None
 */
void SysTick_Init(void);

/**
 * @brief  Returns the number of milliseconds elapsed since SysTick_Init()
 *         was called (i.e. the current value of the internal tick count).
 * @note   Non-blocking. Safe to call from any context. The underlying
 *         counter wraps around every ~49.7 days (2^32 ms); callers doing
 *         elapsed-time math should use unsigned subtraction
 *         (now - previous) so wraparound is handled correctly.
 * @retval Current tick count in milliseconds.
 */
uint32_t systick_get_ms(void);

/**
 * @brief  Blocking delay for the given number of milliseconds.
 * @note   Busy-waits using the SysTick-driven tick count; the CPU does
 *         no other work while waiting. Intended for occasional use
 *         (e.g. startup/settling delays) - NOT for use inside a
 *         scheduler's task functions, since it would block every other
 *         task from running for its duration.
 * @param  ms Number of milliseconds to wait.
 * @retval None
 */
void delay_ms(uint32_t ms);

#endif
