#include "syscall.h"
#include "sched.h"
#include "kprintf.h"
#include "serial.h"
#include "keyboard.h"
#include "guisys.h"
#include "vfs.h"

static int g_cs_logged = 0;

static struct vfs_stat *g_list_arr;
static uint32_t g_list_max;
static uint32_t g_list_n;

static void sys_list_cb(const struct vfs_stat *st, void *ctx)
{
    (void)ctx;
    if (g_list_n < g_list_max)
        g_list_arr[g_list_n++] = *st;
}

uint64_t sys_dispatch(struct int_frame *f)
{
    uint64_t num = f->rax;

    if (!g_cs_logged)
    {
        g_cs_logged = 1;
        kprintf("syscall entry: cs=0x%x rip=0x%llx cpl=%u\n",
                (uint32_t)f->cs, (unsigned long long)f->rip,
                (uint32_t)(f->cs & 3));
    }

    switch (num)
    {
    case SYS_WRITE:
    {
        const char *p = (const char *)f->rdi;
        uint64_t n = f->rsi;
        for (uint64_t i = 0; i < n; i++)
            serial_putc(p[i]);
        f->rax = n;
        return (uint64_t)f;
    }
    case SYS_READ:
    {
        char kc;
        if (keyboard_poll_read(&kc))
        {
            char *dst = (char *)f->rdi;
            dst[0] = kc;
            f->rax = 1;
        }
        else
        {
            f->rax = 0;
        }
        return (uint64_t)f;
    }
    case SYS_GETPID:
    {
        f->rax = sched_current_id();
        return (uint64_t)f;
    }
    case SYS_YIELD:
    {
        return yield_tick(f);
    }
    case SYS_EXIT:
    {
        kprintf("user process pid=%u exit code=%u\n",
                (uint32_t)sched_current_id(), (uint32_t)f->rdi);
        return sched_exit(f);
    }
    case SYS_GUI_WINDOW:
    {
        f->rax = (uint64_t)guisys_window((const char *)f->rdi,
            (int32_t)(f->rsi & 0xFFFFFFFF), (int32_t)(f->rdx & 0xFFFFFFFF),
            (int32_t)(f->rcx & 0xFFFFFFFF), (int32_t)(f->rbx & 0xFFFFFFFF));
        return (uint64_t)f;
    }
    case SYS_GUI_DESTROY:
    {
        f->rax = (uint64_t)guisys_destroy((uint32_t)f->rdi);
        return (uint64_t)f;
    }
    case SYS_GUI_WIDGET:
    {
        f->rax = (uint64_t)guisys_widget((uint32_t)f->rdi,
            (const struct gui_widget_req *)f->rsi);
        return (uint64_t)f;
    }
    case SYS_GUI_POLL:
    {
        f->rax = (uint64_t)guisys_poll((uint32_t)f->rdi,
            (struct gui_event *)f->rsi);
        return (uint64_t)f;
    }
    case SYS_GUI_SET_VISIBLE:
    {
        f->rax = (uint64_t)guisys_set_visible((uint32_t)f->rdi, (int)f->rsi);
        return (uint64_t)f;
    }
    case SYS_GUI_TEXT:
    {
        f->rax = (uint64_t)guisys_text((uint32_t)f->rdi,
            (int32_t)(f->rsi & 0xFFFFFFFF), (int32_t)(f->rdx & 0xFFFFFFFF),
            (const char *)f->rcx);
        return (uint64_t)f;
    }
    case SYS_GUI_LABEL:
    {
        f->rax = (uint64_t)guisys_label((uint32_t)f->rdi,
            (uint32_t)f->rsi, (const char *)f->rdx);
        return (uint64_t)f;
    }
    case SYS_GUI_GET_TEXT:
    {
        f->rax = (uint64_t)guisys_get_text((uint32_t)f->rdi,
            (uint32_t)f->rsi, (char *)f->rdx, (uint32_t)f->rcx);
        return (uint64_t)f;
    }
    case SYS_FOPEN:
    {
        struct task *t = sched_current();
        uint32_t fd = (uint32_t)f->rdi;
        const char *path = (const char *)f->rsi;
        if (fd >= 16 || !path || !t) { f->rax = (uint64_t)-1; return (uint64_t)f; }
        uint32_t n = 0;
        while (n < 127 && path[n]) { t->fd_path[fd][n] = path[n]; n++; }
        t->fd_path[fd][n] = 0;
        f->rax = (uint64_t)fd;
        return (uint64_t)f;
    }
    case SYS_FCLOSE:
    {
        struct task *t = sched_current();
        uint32_t fd = (uint32_t)f->rdi;
        if (fd >= 16 || !t) { f->rax = (uint64_t)-1; return (uint64_t)f; }
        t->fd_path[fd][0] = 0;
        f->rax = 0;
        return (uint64_t)f;
    }
    case SYS_FREAD:
    {
        struct task *t = sched_current();
        uint32_t fd = (uint32_t)f->rdi;
        char *buf = (char *)f->rsi;
        uint32_t max = (uint32_t)f->rdx;
        if (fd >= 16 || !t || !buf) { f->rax = (uint64_t)-1; return (uint64_t)f; }
        if (!t->fd_path[fd][0]) { f->rax = (uint64_t)-1; return (uint64_t)f; }
        int64_t n = vfs_read(t->fd_path[fd], buf, max);
        f->rax = (uint64_t)(n < 0 ? (uint64_t)-1 : (uint64_t)n);
        return (uint64_t)f;
    }
    case SYS_FWRITE:
    {
        struct task *t = sched_current();
        uint32_t fd = (uint32_t)f->rdi;
        const char *buf = (const char *)f->rsi;
        uint32_t len = (uint32_t)f->rdx;
        if (fd >= 16 || !t || !buf) { f->rax = (uint64_t)-1; return (uint64_t)f; }
        if (!t->fd_path[fd][0]) { f->rax = (uint64_t)-1; return (uint64_t)f; }
        int rc = vfs_write(t->fd_path[fd], buf, len);
        f->rax = (uint64_t)rc;
        return (uint64_t)f;
    }
    case SYS_FLIST:
    {
        struct task *t = sched_current();
        const char *path = (const char *)f->rdi;
        struct vfs_stat *out = (struct vfs_stat *)f->rsi;
        uint32_t max = (uint32_t)f->rdx;
        if (!path || !out || !t) { f->rax = 0; return (uint64_t)f; }
        static struct vfs_stat *l_arr;
        static uint32_t l_max;
        static uint32_t l_n;
        l_arr = out;
        l_max = max;
        l_n = 0;
        g_list_arr = l_arr;
        g_list_max = l_max;
        g_list_n = l_n;
        vfs_list(path, sys_list_cb, 0);
        f->rax = (uint64_t)g_list_n;
        return (uint64_t)f;
    }
    case SYS_GUI_SET_TEXT:
    {
        f->rax = (uint64_t)guisys_set_text((uint32_t)f->rdi,
            (uint32_t)f->rsi, (const char *)f->rdx);
        return (uint64_t)f;
    }
    default:
    {
        f->rax = (uint64_t)-1;
        return (uint64_t)f;
    }
    }
}
