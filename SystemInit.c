#include <stm32f1xx.h>

// ---- Register bit definitions ---- 
#define HSEON        (1U << 16)
#define HSERDY       (1U << 17)
#define PLLSRC       (1U << 16)
#define PLLMUL_MASK  (0xFU << 18)   // 4-bit PLLMUL field, bits 21:18 
#define PLLMUL_X9    (0x7U << 18)   // field value 7 -> multiplier x9 
#define PLLRDY       (1U << 25)
#define PLLON        (1U << 24)

#define SW_MASK      (0x3U << 0)   // 2-bit clock switch select field 
#define SW_PLL       (0x2U << 0)   // select PLL as system clock 
#define SWS_MASK     (0x3U << 2)   // 2-bit clock switch status field
#define SWS_PLL      (0x2U << 2)   // status: PLL is in use 

#define FLASH_LATENCY_MASK (0x7U << 0)
#define FLASH_LATENCY_2WS  (0x2U << 0)  // 2 wait states, required for 48MHz < HCLK <= 72MHz 

#define CLOCK_TIMEOUT  100000U  // arbitrary loop-count timeout guard 

uint32_t SystemCoreClock = 8000000U;  // default: HSI on reset, updated by SystemInit() 

/**
 * @brief  Configures system clock: HSE (8MHz) -> PLL x9 -> SYSCLK 72MHz.
 *         Overrides the weak SystemInit() defined in the startup file;
 *         called automatically before main() from the reset handler.
 * @note   Assumes an 8MHz HSE crystal (standard on Blue Pill boards).
 *         If HSE, PLL lock, or the clock switch fails to complete within
 *         the timeout, SystemCoreClock is left at its default value and
 *         the core continues running on HSI (8MHz) as a safe fallback.
 * @retval None
 */
 
void SystemInit(void)
{
    uint32_t timeout;

    // Enable HSE and wait for the oscillator to stabilize 
    RCC->CR |= HSEON;
    timeout = CLOCK_TIMEOUT;
    while (!(RCC->CR & HSERDY)) {
        if (--timeout == 0) {
            return;  // HSE failed to start; stay on default HSI clock 
        }
    }

    // Configure PLL: source = HSE, multiplier = x9 -> 8MHz x 9 = 72MHz 
    RCC->CFGR &= ~(PLLSRC | PLLMUL_MASK);
    RCC->CFGR |=  (PLLSRC | PLLMUL_X9);
    RCC->CR   |= PLLON;

    timeout = CLOCK_TIMEOUT;
    while (!(RCC->CR & PLLRDY)) {
        if (--timeout == 0) {
            return;  // PLL failed to lock; stay on default HSI clock 
        }
    }

    /* Set flash wait states BEFORE switching to the higher clock speed,
       since flash can't be read fast enough at 72MHz with 0 wait states */
    FLASH->ACR &= ~FLASH_LATENCY_MASK;
    FLASH->ACR |=  FLASH_LATENCY_2WS;

    // Request switch to PLL as system clock source 
    RCC->CFGR &= ~SW_MASK;
    RCC->CFGR |=  SW_PLL;

    // Confirm the switch actually completed before trusting the new clock 
    timeout = CLOCK_TIMEOUT;
    while ((RCC->CFGR & SWS_MASK) != SWS_PLL) {
        if (--timeout == 0) {
            return;  // switch failed to confirm; core clock state uncertain 
        }
    }

    /* Core is now confirmed running at 72MHz; reflect this globally
       so any code using SystemCoreClock (e.g. SysTick_Init) is correct */
    SystemCoreClock = 72000000U;
}