#include <stm32f1xx.h>
#include <stdint.h>
#include <string.h>

#define MAX_TASKS 8

typedef void (*task_func_t)(void);
typedef enum {

		task_enabled,
		task_disabled
	
}task_state_t;

typedef struct {
			
		task_func_t  handler;      // function to run
    uint32_t     period_ms;    // how often to run
    uint32_t     last_run_ms;  // last time it ran
    task_state_t state;
    const char  *name;         // for UART shell listing
    uint32_t     run_count;    // stats
    uint32_t     max_exec_us;  // for profiling (optional, stretch)	

}task_t;

void scheduler_init(void);
int  scheduler_add_task(task_func_t fn, uint32_t period_ms, const char *name);
void scheduler_enable_task(int id);
void scheduler_disable_task(int id);
void scheduler_run(void);          // call forever in main loop
void scheduler_print_stats(void);  // UART debug command  

static task_t g_tasks[MAX_TASKS];
static int g_task_count = 0;

int scheduler_add_task(task_func_t fn, uint32_t period_ms, const char *name) {
    if (g_task_count >= MAX_TASKS) return -1;
    g_tasks[g_task_count] = (task_t){
        .handler = fn,
        .period_ms = period_ms,
        .last_run_ms = 0,
        .state = task_enabled,
        .name = name,
        .run_count = 0
    };
    return g_task_count++;
}

void scheduler_run(void) {
    uint32_t now = systick_get_ms();
    for (int i = 0; i < g_task_count; i++) {
        task_t *t = &g_tasks[i];
        if (t->state != task_enabled) continue;
        if ((now - t->last_run_ms) >= t->period_ms) {  // wraparound-safe subtraction
            t->last_run_ms = now;
            t->handler();
            t->run_count++;
        }
    }
}






