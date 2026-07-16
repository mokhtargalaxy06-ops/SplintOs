#ifndef SPLINT_USER_SYSCALL_H
#define SPLINT_USER_SYSCALL_H

#include <stddef.h>

enum splint_open_flags {
    SPLINT_READ = 1,
    SPLINT_WRITE = 2,
    SPLINT_CREATE = 4,
    SPLINT_APPEND = 8,
    SPLINT_TRUNCATE = 16,
};

enum splint_poll_events { SPLINT_POLL_READ = 1, SPLINT_POLL_WRITE = 2 };
struct splint_poll_entry {
    int descriptor;
    unsigned int events;
    unsigned int returned_events;
};

int sys_write(int descriptor, const void *buffer, size_t length);
void sys_exit(int status) __attribute__((noreturn));
int sys_open(const char *path, unsigned int flags);
int sys_read(int descriptor, void *buffer, size_t length);
int sys_close(int descriptor);
int sys_getpid(void);
int sys_spawn(const char *path, const char *const arguments[], size_t count);
int sys_wait(int process, int *status);
int sys_dup2(int source, int destination);
int sys_pipe(int descriptors[2]);

enum splint_descriptor_action_type {
    SPLINT_DESCRIPTOR_DUP2 = 1,
    SPLINT_DESCRIPTOR_CLOSE = 2,
};

struct splint_descriptor_action {
    unsigned int type;
    int source;
    int destination;
};

struct splint_spawn_request {
    const char *path;
    const char *const *arguments;
    unsigned int argument_count;
    const struct splint_descriptor_action *actions;
    unsigned int action_count;
};

int sys_spawn_actions(const struct splint_spawn_request *request);

struct splint_directory_entry {
    char name[32];
    unsigned int type;
    size_t size;
    unsigned short mode;
    unsigned short reserved;
    unsigned int owner;
};

struct splint_memory_info { unsigned int total_kib, free_kib; };
struct splint_process_info { unsigned int process_count, current_pid; };
struct splint_uname { char system[16], release[16], machine[16]; };
struct splint_clock { unsigned int ticks, ticks_per_second; };
struct splint_udp_endpoint {
    unsigned char address[4];
    unsigned short port;
    unsigned short reserved;
};
struct splint_network_config {
    unsigned char address[4], subnet[4], gateway[4], dns[4];
};
struct splint_wall_clock {
    unsigned short year;
    unsigned char month, day, hour, minute, second;
};

int sys_list(const char *path, struct splint_directory_entry *entries, size_t capacity);
int sys_memory_info(struct splint_memory_info *info);
unsigned int sys_uptime(void);
int sys_process_info(struct splint_process_info *info);
void *sys_brk(void *requested);
int sys_yield(void);
int sys_sleep(unsigned int ticks);
int sys_seek(int descriptor, size_t offset);
int sys_fsync(int descriptor);
int sys_unlink(const char *path);
int sys_rename(const char *old_path, const char *new_path);
int sys_log_read(void *buffer, size_t capacity);
int sys_stat(const char *path, struct splint_directory_entry *entry);
int sys_uname(struct splint_uname *identity);
int sys_truncate(int descriptor, size_t size);
int sys_mkdir(const char *path);
int sys_rmdir(const char *path);
int sys_chmod(const char *path, unsigned int mode);
unsigned int sys_getuid(void);
int sys_poll(struct splint_poll_entry *entries, size_t count,
             unsigned int timeout_ticks);
int sys_clock_get(struct splint_clock *clock);
int sys_udp_open(unsigned int local_port);
int sys_udp_send(int descriptor, const struct splint_udp_endpoint *endpoint,
                 const void *data, size_t length);
int sys_udp_receive(int descriptor, struct splint_udp_endpoint *endpoint,
                    void *data, size_t capacity);
int sys_network_config(struct splint_network_config *configuration);
int sys_wall_clock(struct splint_wall_clock *clock);

#endif
