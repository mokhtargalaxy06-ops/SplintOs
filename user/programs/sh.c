#include <splint/syscall.h>

enum { LINE_SIZE = 128, ARGUMENT_COUNT = 8, PATH_SIZE = 64 };

static int write_text(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') ++length;
    return sys_write(1, text, length);
}

static int program_path(char path[PATH_SIZE], const char *name)
{
    path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
    size_t position = 5;
    for (size_t i = 0; name[i] != '\0'; ++i) {
        if (position + 1 >= PATH_SIZE) return -1;
        path[position++] = name[i];
    }
    path[position] = '\0';
    return 0;
}

static int launch(char *line)
{
    char *arguments[ARGUMENT_COUNT];
    size_t count = 0;
    char *cursor = line;
    while (*cursor != '\0' && count < ARGUMENT_COUNT) {
        while (*cursor == ' ') ++cursor;
        if (*cursor == '\0') break;
        arguments[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ') ++cursor;
        if (*cursor != '\0') *cursor++ = '\0';
    }
    if (count == 0) return 0;
    for (size_t split = 1; split + 1 < count; ++split) {
        if (arguments[split][0] != '|' || arguments[split][1] != '\0') continue;
        char producer_path[PATH_SIZE], consumer_path[PATH_SIZE];
        if (program_path(producer_path, arguments[0]) < 0 ||
            program_path(consumer_path, arguments[split + 1]) < 0) return -1;
        arguments[0] = producer_path;
        arguments[split + 1] = consumer_path;
        int pipe_descriptors[2];
        if (sys_pipe(pipe_descriptors) != 0) return -1;
        struct splint_descriptor_action producer_action = {
            SPLINT_DESCRIPTOR_DUP2, pipe_descriptors[1], 1
        };
        struct splint_descriptor_action consumer_action = {
            SPLINT_DESCRIPTOR_DUP2, pipe_descriptors[0], 0
        };
        struct splint_spawn_request producer = {
            producer_path, (const char *const *)arguments, split,
            &producer_action, 1
        };
        struct splint_spawn_request consumer = {
            consumer_path, (const char *const *)(arguments + split + 1),
            count - split - 1, &consumer_action, 1
        };
        int producer_id = sys_spawn_actions(&producer);
        int consumer_id = sys_spawn_actions(&consumer);
        (void)sys_close(pipe_descriptors[0]);
        (void)sys_close(pipe_descriptors[1]);
        if (producer_id < 0 || consumer_id < 0) return -1;
        int producer_status, consumer_status;
        if (sys_wait(producer_id, &producer_status) != producer_id) return -1;
        if (sys_wait(consumer_id, &consumer_status) != consumer_id) return -1;
        return producer_status == 0 ? consumer_status : producer_status;
    }
    int output = -1;
    for (size_t i = 1; i < count; ++i) {
        if (arguments[i][0] != '>' || arguments[i][1] != '\0') continue;
        if (i + 1 >= count) { (void)write_text("sh: missing redirect path\r\n"); return -1; }
        output = sys_open(arguments[i + 1], SPLINT_WRITE | SPLINT_CREATE |
                          SPLINT_TRUNCATE);
        if (output < 0) { (void)write_text("sh: redirect failed\r\n"); return -1; }
        count = i;
        break;
    }
    char path[PATH_SIZE];
    if (program_path(path, arguments[0]) < 0) return -1;
    arguments[0] = path;
    int child;
    if (output >= 0) {
        struct splint_descriptor_action action = {
            SPLINT_DESCRIPTOR_DUP2, output, 1
        };
        struct splint_spawn_request request = {
            path, (const char *const *)arguments, count, &action, 1
        };
        child = sys_spawn_actions(&request);
        (void)sys_close(output);
    } else {
        child = sys_spawn(path, (const char *const *)arguments, count);
    }
    if (child < 0) { (void)write_text("sh: command not found\r\n"); return -1; }
    int status;
    if (sys_wait(child, &status) != child) return -1;
    return status;
}

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    (void)write_text("SplintOS Ring 3 shell online\r\n");
    char startup[] = "cat /README";
    if (launch(startup) == 0)
        (void)write_text("shell: startup command status=0\r\n");
    char descriptor_test[] = "fdtest";
    (void)launch(descriptor_test);
    char echo_test[] = "echo inherited standard output";
    (void)launch(echo_test);
    char pipe_test[] = "pipetest";
    (void)launch(pipe_test);
    char redirect_test[] = "echo redirected output > /tmp/shell-output";
    (void)launch(redirect_test);
    char redirect_read[] = "cat /tmp/shell-output";
    if (launch(redirect_read) == 0)
        (void)write_text("shell: redirection status=0\r\n");
    char pipeline_test[] = "echo pipeline-data | wc";
    if (launch(pipeline_test) == 0)
        (void)write_text("shell: pipeline status=0\r\n");
    char ls_test[] = "ls /bin"; (void)launch(ls_test);
    char mem_test[] = "mem"; (void)launch(mem_test);
    char uptime_test[] = "uptime"; (void)launch(uptime_test);
    char ps_test[] = "ps"; (void)launch(ps_test);
    char heap_test[] = "heaptest"; (void)launch(heap_test);
    char disk_test[] = "disk"; (void)launch(disk_test);
    char line[LINE_SIZE];
    for (;;) {
        (void)write_text("user> ");
        size_t length = 0;
        for (;;) {
            char character;
            if (sys_read(0, &character, 1) != 1) continue;
            if (character == '\n') {
                (void)write_text("\r\n");
                break;
            }
            if (character == '\b' || character == 127) {
                if (length != 0) { --length; (void)write_text("\b \b"); }
            } else if (character >= 32 && character < 127 && length + 1 < sizeof(line)) {
                line[length++] = character;
                (void)sys_write(1, &character, 1);
            }
        }
        line[length] = '\0';
        if (length != 0) (void)launch(line);
    }
}
