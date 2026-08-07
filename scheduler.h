#ifndef SCHEDULER_H
#define SCHEDULER_H

#include<stdint.h>

typedef void (*task_func_t)(void);
void scheduler_init(void);
int  scheduler_add_task(task_func_t fn, uint32_t period_ms, const char *name);
void scheduler_enable_task(int id);
void scheduler_disable_task(int id);
void scheduler_run(void);          // call forever in main loop
void scheduler_print_stats(void);  // UART debug command 

#endif