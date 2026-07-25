#ifndef UNI_SYSCALL_H
#define UNI_SYSCALL_H

#include "idt.h"

#define SYS_WRITE 1
#define SYS_YIELD 2
#define SYS_GETPID 3
#define SYS_EXIT  4
#define SYS_READ  5
#define SYS_GUI_WINDOW      6
#define SYS_GUI_DESTROY     7
#define SYS_GUI_WIDGET      8
#define SYS_GUI_POLL        9
#define SYS_GUI_SET_VISIBLE 10
#define SYS_GUI_TEXT        11
#define SYS_GUI_LABEL       12
#define SYS_GUI_GET_TEXT    13
#define SYS_FOPEN           14
#define SYS_FCLOSE          15
#define SYS_FREAD           16
#define SYS_FWRITE          17
#define SYS_FLIST           18
#define SYS_GUI_SET_TEXT    19

uint64_t sys_dispatch(struct int_frame *f);

#endif
