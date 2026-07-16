#ifndef SPLINTOS_DEVICES_H
#define SPLINTOS_DEVICES_H

#include <stdbool.h>
#include <stdint.h>

enum input_key {
    KEY_NONE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_TAB,
    KEY_ENTER,
};

struct mouse_state {
    int16_t dx;
    int16_t dy;
    bool left;
    bool changed;
};

void devices_init(void);
void devices_poll(void);
enum input_key keyboard_take_key(void);
struct mouse_state mouse_take_state(void);
void serial_write(const char *text);
int console_take_character(void);

#endif
