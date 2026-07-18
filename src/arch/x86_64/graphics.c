#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/graphics.h"

#define PACKED __attribute__((packed))
#define ALIGNED(value) __attribute__((aligned(value)))
enum { WIDTH = 800, HEIGHT = 600, FRAMEBUFFER_FLAG = 1U << 12 };
struct PACKED multiboot_info {
    uint32_t flags, mem_lower, mem_upper, boot_device, command_line;
    uint32_t modules_count, modules_address; uint8_t symbols[16];
    uint32_t memory_map_length, memory_map_address;
    uint32_t drives_length, drives_address, config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_segment, vbe_interface_offset, vbe_interface_length;
    uint64_t framebuffer_address;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t framebuffer_bpp, framebuffer_type, color_info[6];
};
static uint32_t canvas[WIDTH * HEIGHT] ALIGNED(16);
static uint32_t *framebuffer;
static uint32_t pitch, width, height;
static int dirty_left, dirty_top, dirty_right, dirty_bottom;

static void invalidate(int x, int y, int w, int h)
{
    if (x < dirty_left) dirty_left = x;
    if (y < dirty_top) dirty_top = y;
    if (x + w > dirty_right) dirty_right = x + w;
    if (y + h > dirty_bottom) dirty_bottom = y + h;
}
static void rectangle(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0 || x >= (int)width || y >= (int)height ||
        x + w <= 0 || y + h <= 0) return;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int right = x + w > (int)width ? (int)width : x + w;
    int bottom = y + h > (int)height ? (int)height : y + h;
    for (int py = top; py < bottom; ++py)
        for (int px = left; px < right; ++px) canvas[py * WIDTH + px] = color;
    invalidate(left, top, right - left, bottom - top);
}
static int present(void)
{
    if (framebuffer == NULL) return 0;
    if (dirty_left < 0) dirty_left = 0;
    if (dirty_top < 0) dirty_top = 0;
    if (dirty_right > (int)width) dirty_right = (int)width;
    if (dirty_bottom > (int)height) dirty_bottom = (int)height;
    for (int y = dirty_top; y < dirty_bottom; ++y) {
        uint32_t *line = (uint32_t *)((uint8_t *)framebuffer + (size_t)y * pitch);
        for (int x = dirty_left; x < dirty_right; ++x) line[x] = canvas[y * WIDTH + x];
    }
    dirty_left = (int)width; dirty_top = (int)height; dirty_right = dirty_bottom = 0;
    return 1;
}
int x86_64_graphics_init(uint32_t multiboot_address)
{
    if (multiboot_address == 0) return 0;
    const struct multiboot_info *info = (const struct multiboot_info *)(uintptr_t)multiboot_address;
    if ((info->flags & FRAMEBUFFER_FLAG) == 0 || info->framebuffer_bpp != 32 ||
        info->framebuffer_type != 1 || info->framebuffer_width == 0 ||
        info->framebuffer_height == 0 ||
        info->framebuffer_pitch < info->framebuffer_width * 4U ||
        info->framebuffer_address > UINT32_MAX) return 0;
    width = info->framebuffer_width < WIDTH ? info->framebuffer_width : WIDTH;
    height = info->framebuffer_height < HEIGHT ? info->framebuffer_height : HEIGHT;
    pitch = info->framebuffer_pitch; framebuffer = (uint32_t *)(uintptr_t)info->framebuffer_address;
    dirty_left = (int)width; dirty_top = (int)height; dirty_right = dirty_bottom = 0;
    rectangle(0, 0, (int)width, (int)height, UINT32_C(0x081426));
    rectangle(24, 24, 220, 72, UINT32_C(0x2563eb));
    return present();
}
int x86_64_graphics_conformance_test(void)
{
    if (width == 0 || height == 0 || framebuffer == NULL) return 0;
    canvas[0] = 0; canvas[(height - 1U) * WIDTH + width - 1U] = 0;
    rectangle(-4, -4, 8, 8, UINT32_C(0x112233));
    rectangle((int)width - 2, (int)height - 2, 8, 8, UINT32_C(0x445566));
    if (canvas[0] != UINT32_C(0x112233) ||
        canvas[(height - 1U) * WIDTH + width - 1U] != UINT32_C(0x445566)) return 0;
    return present();
}
