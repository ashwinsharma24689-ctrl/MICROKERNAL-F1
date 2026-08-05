#ifndef SYSTEMINIT_H
#define SYSTEMINIT_H

#include <stdint.h>

/**
 * @brief Reflects the current core clock frequency in Hz.
 *        Updated by SystemInit() after the PLL is configured.
 */
extern uint32_t SystemCoreClock;

/**
 * @brief  Configures system clock: HSE (8MHz) -> PLL x9 -> SYSCLK 72MHz.
 *         Called automatically from the startup file before main().
 * @retval None
 */
void SystemInit(void);

#endif 