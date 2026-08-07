#include <stm32f1xx.h>
#include <stdint.h>
#include "SystemInit.h"

// ---- SysTick->CTRL bit definitions ---- 
#define CLK_ENABLE    (1U << 0)   // ENABLE: starts/stops the counter 
#define CLK_SOURCE    (1U << 2)   // CLKSOURCE: 1 = core clock, 0 = core clock / 8 
#define TICKINT_FLAG  (1U << 1)   // TICKINT: 1 = fire interrupt on count-to-zero 

/**
 * @brief Millisecond tick counter, incremented once per SysTick interrupt.
 *        This is the single source of truth for elapsed time in the
 *        system. It must only ever be written by SysTick_Handler() -
 *        every other consumer (delay_ms, the scheduler, etc.) reads it
 *        only through systick_get_ms(), never directly, so that no other
 *        part of the code can accidentally corrupt the timebase.
 *        - static:   confines it to this file (no external writers)
 *        - volatile: prevents the compiler from caching stale copies of
 *                    it in registers across loop iterations, since it
 *                    changes asynchronously from interrupt context
 */
static volatile uint32_t g_tick_ms;

/**
 * @brief  Configures SysTick for a 1ms period and enables its interrupt.
 * @note   LOAD is calculated from SystemCoreClock, so this must be called
 *         after SystemInit() has updated that value to the real core
 *         clock frequency (72MHz), not the HSI reset default.
 * @retval None
 */
void SysTick_Init(void)
{
    /* Reload value for a 1ms period: counts SystemCoreClock/1000 cycles,
       then reloads. The "-1" accounts for the counter also including
       the 0 count itself (LOAD+1 total counts per cycle). */
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;

    /* Start counting from a known state rather than whatever was left
       in VAL from before (e.g. a previous debug session). */
    SysTick->VAL = 0;

    /* Use the core clock directly (matches the LOAD calculation above),
       enable interrupt-on-zero, and start the counter. */
    SysTick->CTRL |= CLK_ENABLE;
    SysTick->CTRL |= CLK_SOURCE;
    SysTick->CTRL |= TICKINT_FLAG;
}

/**
 * @brief  SysTick interrupt handler - fires automatically every 1ms.
 * @note   This function is never called manually anywhere in the code;
 *         the Cortex-M core itself invokes it directly via the vector
 *         table whenever the SysTick counter reaches zero (since
 *         TICKINT is enabled in SysTick_Init()). The name must match
 *         exactly so the linker overrides the weak default handler
 *         defined in the startup file.
 * @note   Kept intentionally minimal - ISRs should do as little work as
 *         possible so they return quickly and don't block other
 *         interrupts or delay the main application.
 * @retval None
 */
void SysTick_Handler(void)
{
    g_tick_ms++;
}

/**
 * @brief  Returns the current millisecond tick count.
 * @note   A 32-bit aligned read on Cortex-M is atomic, so no critical
 *         section is needed here even though g_tick_ms is modified
 *         asynchronously by an interrupt.
 * @retval Milliseconds elapsed since SysTick_Init() was called.
 */
uint32_t systick_get_ms(void)
{
    return g_tick_ms;
}

/**
 * @brief  Busy-waits for the given number of milliseconds.
 * @note   Uses unsigned subtraction (systick_get_ms() - start_time) so
 *         the wait remains correct even if g_tick_ms wraps around
 *         during the delay (unsigned overflow is well-defined modular
 *         arithmetic in C, unlike signed overflow).
 * @note   Blocking - use sparingly. Do not call from within a
 *         cooperative scheduler's task functions, as it will stall
 *         every other task for its entire duration.
 * @param  ms Number of milliseconds to wait.
 * @retval None
 */
void delay_ms(uint32_t ms)
{
    uint32_t start_time = systick_get_ms();
    while ((systick_get_ms() - start_time) < ms) {}
}
