#include "gui.h"
#include "devices.h"
#include "filesystem.h"
#include "network.h"

#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

struct PACKED multiboot_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint8_t syms[16];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr, config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t framebuffer_bpp, framebuffer_type;
    uint8_t color_info[6];
};

static uint32_t *framebuffer;
static uint32_t canvas[800 * 600] __attribute__((aligned(16)));
static uint32_t width, height, pitch;
static bool network_connected;
static uint8_t selected;
static uint8_t file_selected;
static char file_path[128] = "/";
static int cursor_x = 400, cursor_y = 300;
static uint8_t active_window;
static int dirty_left, dirty_top, dirty_right, dirty_bottom;

static void invalidate(int x, int y, int w, int h)
{
    if (x < dirty_left) dirty_left = x;
    if (y < dirty_top) dirty_top = y;
    if (x + w > dirty_right) dirty_right = x + w;
    if (y + h > dirty_bottom) dirty_bottom = y + h;
}

static void present(void)
{
    if (dirty_left < 0) dirty_left = 0;
    if (dirty_top < 0) dirty_top = 0;
    if (dirty_right > (int)width) dirty_right = (int)width;
    if (dirty_bottom > (int)height) dirty_bottom = (int)height;
    for (int y = dirty_top; y < dirty_bottom; ++y) {
        uint32_t *destination = (uint32_t *)((uint8_t *)framebuffer + y * pitch);
        for (int x = dirty_left; x < dirty_right; ++x)
            destination[x] = canvas[y * 800 + x];
    }
    dirty_left = (int)width;
    dirty_top = (int)height;
    dirty_right = dirty_bottom = 0;
}

static const uint8_t font[][7] = {
    [' ' - 32]={0,0,0,0,0,0,0}, ['!' - 32]={4,4,4,4,4,0,4},
    ['.' - 32]={0,0,0,0,0,6,6}, [':' - 32]={0,6,6,0,6,6,0},
    ['-' - 32]={0,0,0,31,0,0,0}, ['>' - 32]={16,8,4,2,4,8,16},
    ['?' - 32]={14,17,1,2,4,0,4}, ['_' - 32]={0,0,0,0,0,0,31},
    ['/' - 32]={1,1,2,4,8,16,16}, ['0' - 32]={14,17,19,21,25,17,14},
    ['1' - 32]={4,12,4,4,4,4,14}, ['2' - 32]={14,17,1,2,4,8,31},
    ['3' - 32]={30,1,1,14,1,1,30}, ['4' - 32]={2,6,10,18,31,2,2},
    ['5' - 32]={31,16,16,30,1,1,30}, ['6' - 32]={14,16,16,30,17,17,14},
    ['7' - 32]={31,1,2,4,8,8,8}, ['8' - 32]={14,17,17,14,17,17,14},
    ['9' - 32]={14,17,17,15,1,1,14}, ['A' - 32]={14,17,17,31,17,17,17},
    ['B' - 32]={30,17,17,30,17,17,30}, ['C' - 32]={14,17,16,16,16,17,14},
    ['D' - 32]={30,17,17,17,17,17,30}, ['E' - 32]={31,16,16,30,16,16,31},
    ['F' - 32]={31,16,16,30,16,16,16}, ['G' - 32]={14,17,16,23,17,17,15},
    ['H' - 32]={17,17,17,31,17,17,17}, ['I' - 32]={14,4,4,4,4,4,14},
    ['J' - 32]={7,2,2,2,2,18,12}, ['K' - 32]={17,18,20,24,20,18,17},
    ['L' - 32]={16,16,16,16,16,16,31}, ['M' - 32]={17,27,21,21,17,17,17},
    ['N' - 32]={17,25,21,19,17,17,17}, ['O' - 32]={14,17,17,17,17,17,14},
    ['P' - 32]={30,17,17,30,16,16,16}, ['Q' - 32]={14,17,17,17,21,18,13},
    ['R' - 32]={30,17,17,30,20,18,17}, ['S' - 32]={15,16,16,14,1,1,30},
    ['T' - 32]={31,4,4,4,4,4,4}, ['U' - 32]={17,17,17,17,17,17,14},
    ['V' - 32]={17,17,17,17,17,10,4}, ['W' - 32]={17,17,17,21,21,21,10},
    ['X' - 32]={17,17,10,4,10,17,17}, ['Y' - 32]={17,17,10,4,4,4,4},
    ['Z' - 32]={31,1,2,4,8,16,31},
    ['a' - 32]={0,0,14,1,15,17,15}, ['b' - 32]={16,16,30,17,17,17,30},
    ['c' - 32]={0,0,14,17,16,17,14}, ['d' - 32]={1,1,15,17,17,17,15},
    ['e' - 32]={0,0,14,17,31,16,14}, ['f' - 32]={6,9,8,28,8,8,8},
    ['g' - 32]={0,0,15,17,15,1,14}, ['h' - 32]={16,16,30,17,17,17,17},
    ['i' - 32]={4,0,12,4,4,4,14}, ['j' - 32]={2,0,6,2,2,18,12},
    ['k' - 32]={16,16,18,20,24,20,18}, ['l' - 32]={12,4,4,4,4,4,14},
    ['m' - 32]={0,0,26,21,21,21,21}, ['n' - 32]={0,0,30,17,17,17,17},
    ['o' - 32]={0,0,14,17,17,17,14}, ['p' - 32]={0,0,30,17,30,16,16},
    ['q' - 32]={0,0,15,17,15,1,1}, ['r' - 32]={0,0,22,25,16,16,16},
    ['s' - 32]={0,0,15,16,14,1,30}, ['t' - 32]={8,8,28,8,8,9,6},
    ['u' - 32]={0,0,17,17,17,19,13}, ['v' - 32]={0,0,17,17,17,10,4},
    ['w' - 32]={0,0,17,17,21,21,10}, ['x' - 32]={0,0,17,10,4,10,17},
    ['y' - 32]={0,0,17,17,15,1,14}, ['z' - 32]={0,0,31,2,4,8,31},
};

static void rectangle(int x, int y, int w, int h, uint32_t color)
{
    if (x < 0 || y < 0 || x + w > (int)width || y + h > (int)height) return;
    for (int py = y; py < y + h; ++py) {
        uint32_t *line = canvas + py * 800;
        for (int px = x; px < x + w; ++px) line[px] = color;
    }
}

static void character(int x, int y, char c, uint32_t color, int scale)
{
    if (c < 32 || c > 'z') c = ' ';
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
            if (font[(unsigned)c - 32][row] & (1U << (4 - col)))
                rectangle(x + col * scale, y + row * scale, scale, scale, color);
}

static void text(int x, int y, const char *value, uint32_t color, int scale)
{
    while (*value) {
        character(x, y, *value++, color, scale);
        x += 6 * scale;
    }
}

static bool file_path_enter(const char *name)
{
    size_t length = 0;
    size_t name_length = 0;
    while (file_path[length] != '\0') ++length;
    while (name[name_length] != '\0') ++name_length;
    size_t separator = length > 1 ? 1 : 0;
    if (length + separator + name_length >= sizeof(file_path)) return false;
    if (separator != 0) file_path[length++] = '/';
    for (size_t i = 0; i < name_length; ++i) file_path[length++] = name[i];
    file_path[length] = '\0';
    return true;
}

static void file_path_parent(void)
{
    size_t length = 0;
    while (file_path[length] != '\0') ++length;
    if (length <= 1) return;
    while (length > 1 && file_path[length - 1] != '/') --length;
    if (length > 1) --length;
    file_path[length] = '\0';
}

static void button(int index, int x, const char *label)
{
    uint32_t fill = selected == index ? 0x2563EB : 0x25334A;
    rectangle(x, 490, 160, 52, selected == index ? 0x60A5FA : 0x334155);
    rectangle(x + 2, 492, 156, 48, fill);
    text(x + 14, 507, label, 0xFFFFFF, 2);
}

static void window_frame(const char *title)
{
    rectangle(135, 120, 530, 350, 0x020617);
    rectangle(137, 122, 526, 346, 0x17243A);
    rectangle(137, 122, 526, 48, 0x1E3A5F);
    text(158, 138, title, 0xF8FAFC, 2);
    rectangle(625, 136, 20, 20, 0xEF4444);
    text(631, 141, "X", 0xFFFFFF, 1);
    text(158, 440, "PRESS ENTER TO CLOSE", 0x64748B, 1);
}

static void draw_terminal(void)
{
    enum { CAPACITY = 2048, ROWS = 22, COLUMNS = 76 };
    char log[CAPACITY];
    size_t count = boot_log_read(log, sizeof(log));
    size_t start = count;
    unsigned lines = 0;
    while (start != 0 && lines <= ROWS) {
        --start;
        if (log[start] == '\n') ++lines;
    }
    if (start != 0) ++start;

    int x = 158, y = 180;
    unsigned column = 0;
    for (size_t i = start; i < count && y < 420; ++i) {
        char value = log[i];
        if (value == '\r') continue;
        if (value == '\n') {
            column = 0; x = 158; y += 10;
            continue;
        }
        if (column >= COLUMNS) continue;
        char glyph[2] = { value, '\0' };
        text(x, y, glyph, 0xD1FAE5, 1);
        x += 6;
        ++column;
    }
    rectangle(150, 432, 500, 28, 0x17243A);
    text(158, 440, "TYPE COMMANDS  LEFT CLOSE", 0x64748B, 1);
}

static bool terminal_log_changed(void)
{
    static char previous[256];
    static size_t previous_count;
    char current[sizeof(previous)];
    size_t count = boot_log_read(current, sizeof(current));
    bool changed = count != previous_count;
    for (size_t i = 0; i < count && !changed; ++i)
        if (current[i] != previous[i]) changed = true;
    if (changed) {
        for (size_t i = 0; i < count; ++i) previous[i] = current[i];
        previous_count = count;
    }
    return changed;
}

static void draw_window(void)
{
    if (active_window == 0) return;
    if (active_window == 1) {
        uint8_t address[4];
        network_address(address);
        window_frame("NETWORK");
        text(165, 205, "ETHERNET", 0x94A3B8, 2);
        text(330, 205, network_connected ? "CONNECTED" : "OFFLINE",
             network_connected ? 0x4ADE80 : 0xF87171, 2);
        text(165, 250, "ADDRESS", 0x94A3B8, 2);
        /* The QEMU fallback and normal DHCP lease use this address. */
        text(330, 250, network_connected ? "10.0.2.15" : "NONE", 0xF8FAFC, 2);
        text(165, 295, "STACK", 0x94A3B8, 2);
        text(330, 295, "ARP IPV4 ICMP UDP DHCP", 0x7DD3FC, 1);
        (void)address;
    } else if (active_window == 2) {
        window_frame("FILES");
        struct vfs_directory_entry entries[8];
        int count = vfs_list(file_path, entries, 8);
        text(165, 180, file_path, 0x94A3B8, 1);
        int y = 200;
        for (int i = 0; i < count; ++i) {
            if (file_selected == (uint8_t)i) {
                rectangle(153, y - 9, 480, 27, 0x2563EB);
            }
            text(165, y, entries[i].type == VFS_DIRECTORY ? "DIR" : "FILE", 0x7DD3FC, 1);
            text(230, y, entries[i].name, 0xF8FAFC, 2);
            y += 32;
        }
        text(165, 413, "UP/DOWN SELECT  ENTER OPEN  LEFT BACK", 0x94A3B8, 1);
    } else if (active_window == 3) {
        window_frame("TERMINAL");
        draw_terminal();
    } else {
        window_frame("ABOUT SPLINTOS");
        text(165, 210, "EXPERIMENTAL X86 OPERATING SYSTEM", 0xF8FAFC, 2);
        text(165, 260, "CREATED IN MOROCCO", 0x4ADE80, 2);
        text(165, 310, "CUSTOM KERNEL GRAPHICS AND NETWORK", 0x94A3B8, 1);
        text(165, 345, "OPEN SOURCE EDUCATIONAL PROJECT", 0x94A3B8, 1);
    }
}

static void draw(void)
{
    rectangle(0, 0, (int)width, (int)height, 0x081426);
    rectangle(0, 0, (int)width, 58, 0x0F1F36);
    rectangle(24, 14, 30, 30, 0x38BDF8);
    rectangle(31, 21, 16, 16, 0x0F1F36);
    text(68, 17, "SPLINTOS", 0xF8FAFC, 3);
    text((int)width - 184, 22, network_connected ? "ONLINE" : "OFFLINE",
         network_connected ? 0x4ADE80 : 0xF87171, 2);

    text(54, 102, "WELCOME", 0xF8FAFC, 4);
    text(55, 143, "YOUR SMALL GRAPHICAL OPERATING SYSTEM", 0x94A3B8, 2);

    rectangle(54, 195, 692, 226, 0x13233A);
    rectangle(55, 196, 690, 224, 0x101C30);
    text(82, 224, "SYSTEM OVERVIEW", 0x7DD3FC, 2);
    text(82, 272, "KERNEL", 0x94A3B8, 2);
    text(250, 272, "RUNNING", 0x4ADE80, 2);
    text(82, 311, "NETWORK", 0x94A3B8, 2);
    text(250, 311, network_connected ? "10.0.2.15" : "NOT FOUND",
         network_connected ? 0x4ADE80 : 0xF87171, 2);
    text(82, 350, "GRAPHICS", 0x94A3B8, 2);
    text(250, 350, "FRAMEBUFFER", 0xF8FAFC, 2);
    text(82, 389, "CONTROL", 0x94A3B8, 2);
    text(250, 389, "ARROWS AND ENTER", 0xF8FAFC, 2);

    button(0, 25, "NETWORK");
    button(1, 220, "FILES");
    button(2, 415, "TERMINAL");
    button(3, 610, "ABOUT");
    text(54, 566, "USE LEFT/RIGHT TO SELECT", 0x64748B, 1);

    draw_window();

    /* High-contrast software pointer. */
    for (int i = 0; i < 15; ++i) {
        rectangle(cursor_x, cursor_y + i, 2 + i / 2, 1, 0xFFFFFF);
    }
    rectangle(cursor_x + 3, cursor_y + 3, 2, 7, 0x0F172A);
    present();
}

bool gui_init(uint32_t address)
{
    const struct multiboot_info *info = (const struct multiboot_info *)(uintptr_t)address;
    if ((info->flags & (1U << 12)) == 0 || info->framebuffer_type != 1 ||
        info->framebuffer_bpp != 32 || info->framebuffer_addr > 0xFFFFFFFFULL ||
        info->framebuffer_addr == 0 || info->framebuffer_width < 800 ||
        info->framebuffer_height < 600 || info->framebuffer_pitch < 800 * 4)
        return false;
    uint64_t framebuffer_end = info->framebuffer_addr +
        (uint64_t)info->framebuffer_pitch * 600U;
    if (framebuffer_end > 0x100000000ULL ||
        framebuffer_end < info->framebuffer_addr) return false;
    framebuffer = (uint32_t *)(uintptr_t)(uint32_t)info->framebuffer_addr;
    width = 800;
    height = 600;
    pitch = info->framebuffer_pitch;
    dirty_left = dirty_top = 0;
    dirty_right = (int)width;
    dirty_bottom = (int)height;
    draw();
    return true;
}

void gui_set_network(bool connected)
{
    network_connected = connected;
    invalidate(0, 0, (int)width, (int)height);
    draw();
}

void gui_poll(void)
{
    bool redraw = false;
    enum input_key key = keyboard_take_key();
    if (key == KEY_LEFT && active_window == 0) {
        selected = selected == 0 ? 3 : (uint8_t)(selected - 1);
        invalidate(50, 485, 700, 62); redraw = true;
    } else if ((key == KEY_RIGHT || key == KEY_TAB) && active_window == 0) {
        selected = (uint8_t)((selected + 1) % 4);
        invalidate(50, 485, 700, 62); redraw = true;
    } else if ((key == KEY_UP || key == KEY_DOWN) && active_window == 2) {
        struct vfs_directory_entry entries[8];
        int count = vfs_list(file_path, entries, 8);
        if (count > 0) {
            if (key == KEY_UP)
                file_selected = file_selected == 0 ? (uint8_t)(count - 1)
                                                   : (uint8_t)(file_selected - 1);
            else
                file_selected = (uint8_t)((file_selected + 1) % count);
            invalidate(145, 180, 500, 245); redraw = true;
        }
    } else if (key == KEY_LEFT && active_window == 2) {
        file_path_parent();
        file_selected = 0;
        invalidate(145, 170, 500, 270); redraw = true;
    } else if (key == KEY_LEFT && active_window == 3) {
        active_window = 0;
        invalidate(125, 110, 550, 370); redraw = true;
    } else if (key == KEY_ENTER) {
        if (active_window == 2) {
            struct vfs_directory_entry entries[8];
            int count = vfs_list(file_path, entries, 8);
            if (count > 0 && file_selected < (uint8_t)count &&
                entries[file_selected].type == VFS_DIRECTORY &&
                file_path_enter(entries[file_selected].name)) {
                file_selected = 0;
                invalidate(145, 170, 500, 270); redraw = true;
            }
        } else if (active_window != 3) {
            active_window = active_window == 0 ? (uint8_t)(selected + 1) : 0;
        }
        invalidate(125, 110, 550, 370); redraw = true;
    }

    struct mouse_state state = mouse_take_state();
    if (state.changed) {
        int old_x = cursor_x, old_y = cursor_y;
        cursor_x += state.dx;
        cursor_y += state.dy;
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > (int)width - 16) cursor_x = (int)width - 16;
        if (cursor_y > (int)height - 16) cursor_y = (int)height - 16;
        if (state.left_pressed && cursor_y >= 490 && cursor_y <= 542) {
            if (cursor_x >= 25 && cursor_x <= 185) { selected = 0; active_window = 1; }
            else if (cursor_x >= 220 && cursor_x <= 380) { selected = 1; active_window = 2; }
            else if (cursor_x >= 415 && cursor_x <= 575) { selected = 2; active_window = 3; }
            else if (cursor_x >= 610 && cursor_x <= 770) { selected = 3; active_window = 4; }
            invalidate(125, 110, 550, 440);
        }
        if (state.left_pressed && active_window != 0 && cursor_x >= 625 && cursor_x <= 645 &&
            cursor_y >= 136 && cursor_y <= 156) {
            active_window = 0;
            invalidate(125, 110, 550, 370);
        }
        invalidate(old_x, old_y, 16, 16);
        invalidate(cursor_x, cursor_y, 16, 16);
        redraw = true;
    }
    if (active_window == 3 && terminal_log_changed()) {
        invalidate(145, 170, 500, 280);
        redraw = true;
    }
    if (redraw) draw();
}
