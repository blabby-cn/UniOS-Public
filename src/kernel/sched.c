#include "sched.h"
#include "kheap.h"
#include "pic.h"
#include "pit.h"
#include "gdt.h"
#include "vmm.h"
#include "pmm.h"
#include "kprintf.h"

extern uint8_t user_blob_start[];
extern uint8_t user_blob_end[];
extern void *memcpy(void *dst, const void *src, unsigned long n);

#define STACK_SIZE (TASK_STACK_PAGES * 4096)
#define FRAME_QWORDS 22

static struct task tasks[MAX_TASKS];
static uint32_t ntasks = 0;
static uint32_t current = 0;
static uint64_t tick_count = 0;
static int g_ready = 0;

void sched_init(void)
{
    ntasks = 1;
    current = 0;
    tasks[0].id = 0;
    tasks[0].state = TASK_RUNNING;
    tasks[0].runs = 0;
    tasks[0].rsp = 0;
    tasks[0].kstack = kmalloc(STACK_SIZE);
    tasks[0].kstack_pages = TASK_STACK_PAGES;
    tasks[0].kstack_top = (uint64_t)tasks[0].kstack + STACK_SIZE;
    tasks[0].name[0] = 'i';
    tasks[0].name[1] = 'd';
    tasks[0].name[2] = 'l';
    tasks[0].name[3] = 'e';
    tasks[0].name[4] = 0;
    g_ready = 1;
}

uint32_t task_create(const char *name, task_entry_t entry)
{
    if (ntasks >= MAX_TASKS)
        return 0;
    uint32_t idx = ntasks;
    ntasks++;
    struct task *t = &tasks[idx];
    t->id = idx;
    t->state = TASK_RUNNING;
    t->runs = 0;
    t->rip = (uint64_t)entry;
    t->kstack = kmalloc(STACK_SIZE);
    if (!t->kstack)
    {
        t->state = TASK_DEAD;
        return 0;
    }
    t->kstack_pages = TASK_STACK_PAGES;
    t->kstack_top = (uint64_t)t->kstack + STACK_SIZE;
    uint64_t *frame = (uint64_t *)((uint8_t *)t->kstack + STACK_SIZE - FRAME_QWORDS * 8);
    for (uint32_t i = 0; i < FRAME_QWORDS; i++)
        frame[i] = 0;
    frame[17] = (uint64_t)entry;
    frame[18] = GDT_SEL_KCODE;
    frame[19] = 0x202;
    frame[20] = (uint64_t)t->kstack + STACK_SIZE;
    frame[21] = GDT_SEL_KDATA;
    t->rsp = (uint64_t)frame;
    uint32_t n = 0;
    while (name[n] && n < 15)
    {
        t->name[n] = name[n];
        n++;
    }
    t->name[n] = 0;
    return idx;
}

uint32_t task_create_user(const char *name)
{
    if (ntasks >= MAX_TASKS)
        return 0;
    uint32_t idx = ntasks;
    ntasks++;
    struct task *t = &tasks[idx];
    t->id = idx;
    t->state = TASK_RUNNING;
    t->runs = 0;
    t->rip = 0x100000000ULL;

    t->kstack = kmalloc(STACK_SIZE);
    if (!t->kstack)
    {
        t->state = TASK_DEAD;
        return 0;
    }
    t->kstack_pages = TASK_STACK_PAGES;
    t->kstack_top = (uint64_t)t->kstack + STACK_SIZE;

    uint64_t base = 0x8000000000ULL;
    uint64_t code_va = base;
    uint64_t stack_top_va = base + 0x2000ULL;
    void *cp = pmm_alloc();
    void *sp = pmm_alloc();
    if (!cp || !sp)
    {
        t->state = TASK_DEAD;
        return 0;
    }
    vmm_map(code_va, (uint64_t)(unsigned long)cp,
            VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
    vmm_map(stack_top_va - 4096, (uint64_t)(unsigned long)sp,
            VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);

    uint64_t len = (uint64_t)&user_blob_end - (uint64_t)&user_blob_start;
    memcpy((void *)code_va, user_blob_start, len);

    uint64_t *frame = (uint64_t *)((uint8_t *)t->kstack + STACK_SIZE - FRAME_QWORDS * 8);
    for (uint32_t i = 0; i < FRAME_QWORDS; i++)
        frame[i] = 0;
    frame[17] = code_va;
    frame[18] = GDT_SEL_UCODE;
    frame[19] = 0x202;
    frame[20] = stack_top_va;
    frame[21] = GDT_SEL_UDATA;
    t->rsp = (uint64_t)frame;

    uint32_t n = 0;
    while (name[n] && n < 15)
    {
        t->name[n] = name[n];
        n++;
    }
    t->name[n] = 0;
    return idx;
}

uint32_t task_create_user_blob(const char *name, const uint8_t *blob, uint64_t sz, uint64_t code_va)
{
    if (ntasks >= MAX_TASKS)
        return 0;
    uint32_t idx = ntasks;
    ntasks++;
    struct task *t = &tasks[idx];
    t->id = idx;
    t->state = TASK_RUNNING;
    t->runs = 0;
    t->rip = 0x100000000ULL;

    t->kstack = kmalloc(STACK_SIZE);
    if (!t->kstack) { t->state = TASK_DEAD; return 0; }
    t->kstack_pages = TASK_STACK_PAGES;
    t->kstack_top = (uint64_t)t->kstack + STACK_SIZE;

    uint64_t pages_needed = (sz + 4095) / 4096 + 1;
    for (uint64_t p = 0; p < pages_needed; p++)
    {
        void *phys = pmm_alloc();
        if (!phys) { t->state = TASK_DEAD; return 0; }
        vmm_map(code_va + p * 4096, (uint64_t)(unsigned long)phys,
                VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
    }
    void *sp = pmm_alloc();
    uint64_t stack_top_va = code_va + pages_needed * 4096 + 4096;
    if (!sp) { t->state = TASK_DEAD; return 0; }
    vmm_map(stack_top_va - 4096, (uint64_t)(unsigned long)sp,
            VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);

    for (uint64_t i = 0; i < pages_needed * 4096; i++)
        ((volatile uint8_t *)code_va)[i] = 0;
    for (uint64_t i = 0; i < 4096; i++)
        ((volatile uint8_t *)(stack_top_va - 4096))[i] = 0;
    memcpy((void *)code_va, blob, sz);

    uint64_t *frame = (uint64_t *)((uint8_t *)t->kstack + STACK_SIZE - FRAME_QWORDS * 8);
    for (uint32_t i = 0; i < FRAME_QWORDS; i++) frame[i] = 0;
    frame[17] = code_va;
    frame[18] = GDT_SEL_UCODE;
    frame[19] = 0x202;
    frame[20] = stack_top_va;
    frame[21] = GDT_SEL_UDATA;
    t->rsp = (uint64_t)frame;

    uint32_t n = 0;
    while (name[n] && n < 15) { t->name[n] = name[n]; n++; }
    t->name[n] = 0;
    return idx;
}

static uint32_t pick_next(void)
{
    for (uint32_t i = 1; i <= ntasks; i++)
    {
        uint32_t idx = (current + i) % ntasks;
        if (tasks[idx].state == TASK_RUNNING)
            return idx;
    }
    return current;
}

uint64_t timer_tick(struct int_frame *f)
{
    if (!g_ready)
    {
        pic_eoi(0);
        return (uint64_t)f;
    }
    tick_count++;
    pit_tick_inc();
    pic_eoi(0);
    struct task *prev = &tasks[current];
    prev->rsp = (uint64_t)f;
    uint32_t next = pick_next();
    current = next;
    tss_set_rsp0(tasks[next].kstack_top);
    return tasks[next].rsp;
}

uint64_t yield_tick(struct int_frame *f)
{
    if (!g_ready)
        return (uint64_t)f;
    struct task *prev = &tasks[current];
    prev->rsp = (uint64_t)f;
    uint32_t next = pick_next();
    current = next;
    tss_set_rsp0(tasks[next].kstack_top);
    return tasks[next].rsp;
}

uint64_t sched_exit(struct int_frame *f)
{
    (void)f;
    struct task *cur = &tasks[current];
    cur->state = TASK_DEAD;
    uint32_t next = pick_next();
    current = next;
    tss_set_rsp0(tasks[next].kstack_top);
    return tasks[next].rsp;
}

void task_yield(void)
{
    __asm__ volatile("int $0x40");
}

uint64_t sched_ticks(void)
{
    return tick_count;
}

uint32_t sched_task_count(void)
{
    return ntasks;
}

uint32_t sched_current_id(void)
{
    return current;
}

struct task *sched_current(void)
{
    return &tasks[current];
}
