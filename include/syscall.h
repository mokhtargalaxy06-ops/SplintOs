#ifndef SPLINTOS_SYSCALL_H
#define SPLINTOS_SYSCALL_H

struct interrupt_frame;

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);

#endif
