#ifndef SPLINTOS_DEVICES_H
#define SPLINTOS_DEVICES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum input_key {
    KEY_NONE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_TAB,
    KEY_ENTER,
};

struct mouse_state {
    int16_t dx;
    int16_t dy;
    bool left;
    bool left_pressed;
    bool changed;
};

struct wall_clock {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
};

void devices_init(void);
void devices_poll(void);
enum input_key keyboard_take_key(void);
struct mouse_state mouse_take_state(void);
void serial_write(const char *text);
size_t boot_log_read(char *buffer, size_t capacity);
int console_take_character(void);
bool console_has_character(void);
bool devices_wall_clock(struct wall_clock *clock);
bool devices_wall_clock_seconds(uint32_t *seconds);

#endif
