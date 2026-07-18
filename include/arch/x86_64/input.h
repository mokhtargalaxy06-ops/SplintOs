#ifndef SPLINTOS_ARCH_X86_64_INPUT_H
#define SPLINTOS_ARCH_X86_64_INPUT_H
#include <stdint.h>
enum x86_64_input_type { X86_64_INPUT_KEY, X86_64_INPUT_POINTER };
struct x86_64_input_event {
    uint64_t timestamp;
    int32_t value_x, value_y;
    uint16_t code;
    uint8_t type, pressed;
};
void x86_64_input_init(void);
int x86_64_input_push(const struct x86_64_input_event *event);
int x86_64_input_pop(struct x86_64_input_event *event);
int x86_64_input_conformance_test(void);
int x86_64_ps2_keyboard_init(void);
void x86_64_ps2_keyboard_irq(void);
int x86_64_ps2_mouse_init(void);
void x86_64_ps2_mouse_irq(void);
#endif
