#ifndef SPLINTOS_GUI_H
#define SPLINTOS_GUI_H

#include <stdbool.h>
#include <stdint.h>

bool gui_init(uint32_t multiboot_info_address);
void gui_set_network(bool connected);
void gui_poll(void);

#endif
