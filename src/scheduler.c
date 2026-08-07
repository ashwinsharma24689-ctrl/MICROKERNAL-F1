#include <stdint.h>
#include <stdio.h>
#include "scheduler.h"
#include "SysTick.h"
#include "USART1.h"

#define MAX_TASKS 8

// Whether a task is currently eligible to run. 
typedef enum {
    task_enabled,
    task_disabled
} task_state_t;

/**
 * Task Control Block - everything the scheduler needs to decide
 * whether, when, and how often to run a given task.
 */
typedef struct {
    task_func_t  handler;      /* function to run */
    uint32_t     period_ms;    /* how often to run */
    uint32_t     last_run_ms;  /* tick count at last run */
    task_state_t state;
    const char  *name;         /* for scheduler_print_stats() */
    uint32_t     run_count;    /* stats */
} task_t;

static task_t g_tasks[MAX_TASKS];
static int    g_task_count;

/**
 * @brief  Resets the scheduler to a known empty state.
 * @retval None
 */
void scheduler_init(void)
{
    g_task_count = 0;
}

/**
 * @brief  Registers a new periodic task with the scheduler.
 * @retval Task ID (>= 0), or -1 if the task table is full.
 */
int scheduler_add_task(task_func_t fn, uint32_t period_ms, const char *name)
{
    if (g_task_count >= MAX_TASKS)
    {
        return -1;
    }

    g_tasks[g_task_count] = (task_t){
        .handler     = fn,
        .period_ms   = period_ms,
        .last_run_ms = 0,
        .state       = task_enabled,
        .name        = name,
        .run_count   = 0
    };

    return g_task_count++;
}

/**
 * @brief  Enables a previously registered task.
 * @retval None
 */
void scheduler_enable_task(int id)
{
    if (id < 0 || id >= g_task_count)
    {
        return;  /* invalid ID - silently ignored */
    }

    g_tasks[id].state = task_enabled;
}

/**
 * @brief  Disables a task, preventing it from running until re-enabled.
 * @retval None
 */
void scheduler_disable_task(int id)
{
    if (id < 0 || id >= g_task_count)
    {
        return;  /* invalid ID - silently ignored */
    }

    g_tasks[id].state = task_disabled;
}

/**
 * @brief  Checks all registered tasks and runs any whose period has
 *         elapsed since their last run.
 * @note   Uses wraparound-safe unsigned subtraction (now - last_run_ms),
 *         so timing stays correct even across the ~49.7 day tick
 *         counter overflow.
 * @note   Deliberately allows drift: last_run_ms is set to the actual
 *         current time, not the ideal scheduled time. A task that runs
 *         slightly late this cycle is scheduled relative to *when it
 *         actually ran*, not the original fixed timeline. This is an
 *         acceptable tradeoff for tasks like blinking an LED or polling
 *         a UART shell, which have no hard real-time requirement; a
 *         drift-free ("ideal timeline") scheduler would be needed for
 *         tasks requiring a precisely maintained sample rate.
 * @retval None
 */
void scheduler_run(void)
{
    uint32_t now = systick_get_ms();

    for (int i = 0; i < g_task_count; i++)
    {
        task_t *t = &g_tasks[i];

        if (t->state != task_enabled)
        {
            continue;
        }

        if ((now - t->last_run_ms) >= t->period_ms)
        {
            t->last_run_ms = now;
            t->handler();
            t->run_count++;
        }
    }
}

/**
 * @brief  Reports each task's name, state, and run count over USART1.
 * @note   Formatting overflow is handled by snprintf() itself (it never
 *         writes past the buffer); a truncated line is skipped rather
 *         than aborting the whole report, so one oversized task name
 *         can't hide every task after it. Buffer overflow protection on
 *         the wire is handled separately, inside usart_send_string().
 * @retval None
 */
void scheduler_print_stats(void)
{
    char line[64];

    for (int i = 0; i < g_task_count; i++)
    {
        task_t *t = &g_tasks[i];
        const char *state_str = (t->state == task_enabled) ? "enabled" : "disabled";

        /* snprintf returns the length the formatted string WOULD have
           been with no size limit - not counting the null terminator.
           If that's >= sizeof(line), the real output was truncated. */
        int len = snprintf(line, sizeof(line), " [%d] %-12s %-8s runs=%u\r\n",
                            i, t->name, state_str, t->run_count);

        if (len >= (int)sizeof(line))
        {
            continue;  /* this line didn't fit - skip it, keep reporting the rest */
        }

        usart_send_string(line);
    }
}
