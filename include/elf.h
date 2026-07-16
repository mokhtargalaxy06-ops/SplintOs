#ifndef SPLINTOS_ELF_H
#define SPLINTOS_ELF_H

#include <stddef.h>
#include "scheduler.h"

int elf_load_process(const char *path, const char *name);
int elf_load_process_args(const char *path, const char *name,
                          size_t argument_count, const char *const arguments[]);
int elf_load_process_actions(const char *path, const char *name,
                             size_t argument_count, const char *const arguments[],
                             const struct process_descriptor_action *actions,
                             size_t action_count);

#endif
