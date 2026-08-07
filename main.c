#include <stm32f1xx.h>
#include "scheduler.h"
#include "SystemInit.h"
#include "SysTick.h"
#include "USART1.h"

/* ---- Task periods (ms) ---- */
#define BLINK_PERIOD_MS     500   /* visibly obvious blink rate */
#define SHELL_PERIOD_MS     20    /* frequent poll - keeps echo feeling responsive */
#define HEARTBEAT_PERIOD_MS 3000  /* slow, periodic proof-of-life - avoids spamming the terminal */

static void gpio_led_init(void);
static void task_blink_led(void);
static void task_shell(void);
static void task_heartbeat(void);

int main(void)
{
    /* Boot sequence - order matters:
       1. SystemInit()   - clock must be running at 72MHz before anything
                            downstream calculates timing off SystemCoreClock
       2. SysTick_Init() - tick source must be live before the scheduler
                            (or anything else) can measure elapsed time
       3. USART1_Init()  - baud rate depends on the now-correct PCLK2
       4. scheduler_init() / gpio_led_init() - one-time application setup */
    SystemInit();
    SysTick_Init();
    USART1_Init();
    scheduler_init();
    gpio_led_init();

    /* Task IDs are captured so they can later be enabled/disabled at
       runtime (e.g. via a future shell command). */
    int blink_id     = scheduler_add_task(task_blink_led, BLINK_PERIOD_MS, "blink");
    int shell_id     = scheduler_add_task(task_shell, SHELL_PERIOD_MS, "shell");
    int heartbeat_id = scheduler_add_task(task_heartbeat, HEARTBEAT_PERIOD_MS, "heartbeat");

    /* main() stays intentionally minimal - all dispatch logic lives in
       the scheduler; this loop never needs to change as tasks are added. */
    while (1)
    {
        scheduler_run();
    }
}

/**
 * @brief  One-time GPIO setup for the onboard LED (PC13).
 * @note   Called once from main() before the scheduler starts - must
 *         not be called from inside a task, since it only needs to run
 *         once, not on every dispatch cycle.
 * @retval None
 */
static void gpio_led_init(void)
{
    RCC->APB2ENR |= (1U << 4);   /* GPIOC clock enable */
    GPIOC->CRH &= ~(3U << 20);   /* clear MODE13 */
    GPIOC->CRH |= (1U << 20);    /* MODE13 = 01: output, 10MHz */
}

/**
 * @brief  Toggles the onboard LED (PC13).
 * @note   Scheduler task - called once every BLINK_PERIOD_MS. Contains
 *         only the per-cycle toggle; setup lives in gpio_led_init().
 * @retval None
 */
static void task_blink_led(void)
{
    GPIOC->ODR ^= (1U << 13);
}

/**
 * @brief  Echoes back a single received UART byte, if one is available.
 * @note   Scheduler task - polled every SHELL_PERIOD_MS. Non-blocking:
 *         does nothing if the RX buffer is empty. `received` stays an
 *         int until after the -1 (empty) check, so the sentinel value
 *         is never confused with a genuine byte in the 0-255 range.
 * @retval None
 */
static void task_shell(void)
{
    int received = usart_read_byte();
    if (received == -1)
    {
        return;  /* nothing to echo this cycle */
    }

    char buf[2];
    buf[0] = (char)received;
    buf[1] = '\0';
    usart_send_string(buf);
}

/**
 * @brief  Sends a periodic proof-of-life message over UART.
 * @note   Scheduler task - called once every HEARTBEAT_PERIOD_MS. Kept
 *         slow deliberately, so it doesn't flood the terminal - this is
 *         a liveness indicator, not a data stream.
 * @retval None
 */
static void task_heartbeat(void)
{
    usart_send_string(".\r\n");
}