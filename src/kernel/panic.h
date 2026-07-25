#ifndef UNI_PANIC_H
#define UNI_PANIC_H

#include "idt.h"

void panic_exception(struct int_frame *f) __attribute__((noreturn));

#endif
