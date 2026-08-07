#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/** Function signature every task must implement. Called with no
 *  arguments, returns nothing - a task is expected to do its work and
 *  return quickly, since this is a cooperative scheduler with no
 *  preemption: one slow task delays every other task's dispatch. */
typedef void (*task_func_t)(void);

/**
 * @brief  Resets the scheduler to a known empty state.
 * @note   Safe to call even though static storage is already
 *         zero-initialized by the C runtime - this makes the "ready"
 *         state explicit rather than relying on that implicitly.
 * @retval None
 */
void scheduler_init(void);

/**
 * @brief  Registers a new periodic task with the scheduler.
 * @param  fn        Function to call each time the task's period elapses.
 * @param  period_ms How often to run the task, in milliseconds.
 * @param  name      Short display name (for scheduler_print_stats()).
 *                    Not copied - the pointer must remain valid for the
 *                    lifetime of the program (e.g. a string literal).
 * @retval Task ID (>= 0) to use with scheduler_enable_task() /
 *         scheduler_disable_task(), or -1 if the task table is full.
 */
int scheduler_add_task(task_func_t fn, uint32_t period_ms, const char *name);

/**
 * @brief  Enables a previously registered task, allowing it to run again.
 * @param  id Task ID returned by scheduler_add_task(). Out-of-range IDs
 *            are silently ignored.
 * @retval None
 */
void scheduler_enable_task(int id);

/**
 * @brief  Disables a task, preventing it from running until re-enabled.
 * @param  id Task ID returned by scheduler_add_task(). Out-of-range IDs
 *            are silently ignored.
 * @retval None
 */
void scheduler_disable_task(int id);

/**
 * @brief  Checks all registered tasks and runs any whose period has
 *         elapsed since their last run.
 * @note   Non-blocking overall, but each individual task's handler is
 *         called synchronously and must return promptly - this
 *         function does not preempt or time-slice tasks.
 * @retval None
 */
void scheduler_run(void);

/**
 * @brief  Reports each task's name, enabled/disabled state, and run
 *         count over USART1, one line per task.
 * @note   Depends on USART1 being initialized before this is called.
 * @retval None
 */
void scheduler_print_stats(void);

#endif