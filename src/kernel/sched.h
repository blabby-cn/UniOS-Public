#ifndef UNI_SCHED_H
#define UNI_SCHED_H

#include <stdint.h>
#include "idt.h"
#include "gdt.h"

typedef void (*task_entry_t)(void);

#define TASK_RUNNING 0
#define TASK_DEAD 1

#define MAX_TASKS 16
#define TASK_STACK_PAGES 2

struct task
{
    uint64_t rsp;
    uint64_t rip;
    uint32_t id;
    uint32_t state;
    uint64_t runs;
    char name[16];
    void *kstack;
    uint32_t kstack_pages;
    uint64_t kstack_top;
    char fd_path[16][128];
};

void sched_init(void);
uint32_t task_create(const char *name, task_entry_t entry);
uint32_t task_create_user(const char *name);
uint32_t task_create_user_blob(const char *name, const uint8_t *blob, uint64_t sz, uint64_t code_va);
void task_yield(void);
uint64_t sched_exit(struct int_frame *f);
uint64_t sched_ticks(void);
uint32_t sched_task_count(void);
uint32_t sched_current_id(void);
struct task *sched_current(void);

uint64_t timer_tick(struct int_frame *f);
uint64_t yield_tick(struct int_frame *f);

#endif
