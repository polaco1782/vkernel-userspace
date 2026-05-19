/*
 * vkernel userspace - shell
 * Copyright (C) 2026 vkernel authors
 *
 * shell.c - Freestanding userspace shell for vkernel
 *
 * Build: see Makefile (Linux) or shell.vcxproj (Visual Studio).
 * Run:   launched automatically by the kernel as shell.elf / shell.exe
 */

#include "../include/vk.h"

#if defined(_MSC_VER)
#pragma warning(disable: 4702) /* unreachable code (infinite shell loop) */
#endif

/* Freestanding: define what we need ourselves. */
#define VK_NULL ((void*)0)
#define SHELL_PROMPT "vk> "
#define SHELL_HISTORY_MAX 8
#define SHELL_LINE_MAX 256
#define SHELL_PATH_MAX 256
#define SHELL_FS_RESPONSE_MAX 4096
#define SHELL_FS_ITEMS_MAX 64
#define SHELL_FS_ITEM_MAX 96

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static const char* SHELL_COMMANDS[] = {
    "help",
    "version",
    "pwd",
    "cd",
    "ls",
    "cat",
    "clear",
    "reboot",
    "run",
    "drvload",
    "exit",
    VK_NULL,
};

/* -------------------------------------------------------------------------
 * Freestanding string helpers
 * ---------------------------------------------------------------------- */

static vk_usize vk_strlen(const char* s)
{
    vk_usize n = 0;
    while (s[n]) ++n;
    return n;
}

static int vk_strcmp(const char* a, const char* b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int vk_has_suffix(const char* s, const char* suffix)
{
    vk_usize s_len = vk_strlen(s);
    vk_usize suffix_len = vk_strlen(suffix);

    if (suffix_len > s_len)
        return 0;

    return vk_strcmp(s + (s_len - suffix_len), suffix) == 0;
}

/* Returns 1 if `s` starts with `prefix`, 0 otherwise. */
static int vk_has_prefix(const char* s, const char* prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

/* Skip leading whitespace. */
static const char* vk_skip_spaces(const char* s)
{
    while (*s == ' ' || *s == '\t') ++s;
    return s;
}


static int shell_has_framebuffer(void)
{
    vk_framebuffer_info_t fb;
    VK_CALL(framebuffer_info, &fb);
    return fb.valid != 0;
}

static int shell_cmdline_has_flag(const char* flag)
{
    char cmdline[256];
    const char* p;

    if (!vk_get_api()->vk_get_cmdline)
        return 0;

    vk_get_api()->vk_get_cmdline(cmdline, sizeof(cmdline));
    p = cmdline;

    while (*p != '\0') {
        char token[64];
        vk_usize len = 0;
        char quote = 0;

        while (*p == ' ' || *p == '\t')
            ++p;
        if (*p == '\0')
            break;

        if (*p == '"' || *p == '\'')
            quote = *p++;

        while (*p != '\0') {
            char ch = *p;
            if (quote) {
                if (ch == quote) {
                    ++p;
                    break;
                }
            } else if (ch == ' ' || ch == '\t') {
                break;
            }

            if (len + 1 < sizeof(token))
                token[len++] = ch;
            ++p;
        }

        token[len] = '\0';
        if (vk_strcmp(token, flag) == 0)
            return 1;
    }

    return 0;
}

static void shell_put_spaces(vk_usize count)
{
    while (count-- > 0)
        VK_CALL(putc, ' ');
}

static void shell_put_padded(const char* s, vk_usize width)
{
    vk_usize len = vk_strlen(s);
    VK_CALL(puts, s);
    if (len < width)
        shell_put_spaces(width - len);
}

static void shell_put_dec_width(vk_u64 value, vk_usize width)
{
    char buf[21];
    vk_usize len = 0;

    if (value == 0) {
        buf[len++] = '0';
    } else {
        while (value > 0 && len < sizeof(buf)) {
            buf[len++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }

    if (len < width)
        shell_put_spaces(width - len);

    while (len > 0)
        VK_CALL(putc, buf[--len]);
}

static char s_history[SHELL_HISTORY_MAX][SHELL_LINE_MAX];
static vk_usize s_history_count = 0;

static void shell_copy_line(char* dst, vk_usize dst_cap, const char* src)
{
    vk_usize i = 0;
    if (dst_cap == 0) return;
    while (src[i] && i < dst_cap - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void shell_history_add(const char* line)
{
    if (line[0] == '\0') return;
    if (s_history_count > 0 &&
        vk_strcmp(s_history[s_history_count - 1], line) == 0)
        return;

    if (s_history_count < SHELL_HISTORY_MAX) {
        shell_copy_line(s_history[s_history_count++], SHELL_LINE_MAX, line);
        return;
    }

    for (vk_usize i = 1; i < SHELL_HISTORY_MAX; ++i)
        shell_copy_line(s_history[i - 1], SHELL_LINE_MAX, s_history[i]);
    shell_copy_line(s_history[SHELL_HISTORY_MAX - 1], SHELL_LINE_MAX, line);
}

static void shell_redraw_line(const char* prompt, const char* buf, vk_usize old_len)
{
    VK_CALL(putc, '\r');
    VK_CALL(puts, prompt);
    for (vk_usize i = 0; i < old_len; ++i)
        VK_CALL(putc, ' ');
    VK_CALL(putc, '\r');
    VK_CALL(puts, prompt);
    VK_CALL(puts, buf);
}

static int shell_command_starts_with(const char* command, const char* prefix)
{
    while (*prefix) {
        if (*command++ != *prefix++)
            return 0;
    }
    return 1;
}

static void shell_complete_line(char* buf, vk_usize max, vk_usize* pos, const char* prompt)
{
    vk_usize token_len = 0;
    vk_usize match_count = 0;
    const char* match = VK_NULL;

    while (token_len < *pos && buf[token_len] != ' ' && buf[token_len] != '\t')
        ++token_len;
    if (token_len != *pos)
        return;

    for (vk_usize i = 0; SHELL_COMMANDS[i] != VK_NULL; ++i) {
        if (shell_command_starts_with(SHELL_COMMANDS[i], buf)) {
            match = SHELL_COMMANDS[i];
            ++match_count;
        }
    }

    if (match_count == 1 && match != VK_NULL) {
        vk_usize i = *pos;
        while (match[i] && *pos < max - 1) {
            buf[*pos] = match[i++];
            VK_CALL(putc, buf[*pos]);
            ++(*pos);
        }
        if (*pos < max - 1) {
            buf[(*pos)++] = ' ';
            VK_CALL(putc, ' ');
        }
        buf[*pos] = '\0';
        return;
    }

    if (match_count > 1) {
        VK_CALL(putc, '\n');
        for (vk_usize i = 0; SHELL_COMMANDS[i] != VK_NULL; ++i) {
            if (shell_command_starts_with(SHELL_COMMANDS[i], buf)) {
                VK_CALL(puts, SHELL_COMMANDS[i]);
                VK_CALL(puts, "  ");
            }
        }
        VK_CALL(putc, '\n');
        VK_CALL(puts, prompt);
        VK_CALL(puts, buf);
    }
}

/* -------------------------------------------------------------------------
 * Console I/O
 * ---------------------------------------------------------------------- */

/*
 * Read a line from the console into `buf` (max `max` bytes including NUL).
 * Handles backspace/DEL and filters non-printable characters.
 * Returns the number of characters stored (not counting NUL).
 */
static vk_usize console_getline(char* buf, vk_usize max, const char* prompt)
{
    vk_usize pos = 0;
    vk_usize history_index = s_history_count;
    vk_usize old_len = 0;

    while (pos < max - 1) {
        char c = VK_CALL(getc);

        if (c == '\r' || c == '\n') {
            VK_CALL(putc, '\n');
            break;
        }

        if (c == '\t') {
            shell_complete_line(buf, max, &pos, prompt);
            old_len = pos;
            continue;
        }

        if (c == 27) {
            char c1 = VK_CALL(try_getc);
            char c2 = VK_CALL(try_getc);
            if (c1 == '[' && (c2 == 'A' || c2 == 'B')) {
                if (c2 == 'A' && history_index > 0) {
                    --history_index;
                    shell_copy_line(buf, max, s_history[history_index]);
                    pos = vk_strlen(buf);
                    shell_redraw_line(prompt, buf, old_len);
                    old_len = pos;
                } else if (c2 == 'B') {
                    if (history_index + 1 < s_history_count) {
                        ++history_index;
                        shell_copy_line(buf, max, s_history[history_index]);
                    } else {
                        history_index = s_history_count;
                        buf[0] = '\0';
                    }
                    pos = vk_strlen(buf);
                    shell_redraw_line(prompt, buf, old_len);
                    old_len = pos;
                }
            }
            continue;
        }

        if ((c == 0x7F || c == '\b') && pos > 0) {
            --pos;
            buf[pos] = '\0';
            VK_CALL(puts, "\b \b");
            old_len = pos;
            continue;
        }

        if (c >= ' ' && c < 0x7F) {
            buf[pos++] = c;
            buf[pos] = '\0';
            VK_CALL(putc, c);
            old_len = pos;
        }
    }

    buf[pos] = '\0';
    shell_history_add(buf);
    return pos;
}

/* -------------------------------------------------------------------------
 * Shell paths and filesystem helpers
 * ---------------------------------------------------------------------- */

static char s_root_path[SHELL_PATH_MAX] = "/";
static char s_cwd[SHELL_PATH_MAX] = "/";

static int shell_is_separator(char ch)
{
    return ch == '/' || ch == '\\';
}

static int shell_path_has_spaces(const char* path)
{
    while (*path != '\0') {
        if (*path == ' ' || *path == '\t')
            return 1;
        ++path;
    }
    return 0;
}

static const char* shell_basename(const char* path)
{
    const char* base = path;

    while (*path != '\0') {
        if (shell_is_separator(*path))
            base = path + 1;
        ++path;
    }

    return base;
}

static int shell_normalize_absolute_path(const char* path, char* out, vk_usize out_cap)
{
    vk_usize component_starts[32];
    vk_usize component_count = 0;
    vk_usize out_len = 1;

    if (path == VK_NULL || out == VK_NULL || out_cap < 2)
        return 0;

    out[0] = '/';
    out[1] = '\0';

    while (*path != '\0') {
        char component[64];
        vk_usize component_len = 0;
        vk_usize start = 0;

        while (shell_is_separator(*path))
            ++path;
        if (*path == '\0')
            break;

        while (*path != '\0' && !shell_is_separator(*path)) {
            if (component_len + 1 >= sizeof(component))
                return 0;
            component[component_len++] = *path++;
        }
        component[component_len] = '\0';

        if (component_len == 1 && component[0] == '.')
            continue;

        if (component_len == 2 && component[0] == '.' && component[1] == '.') {
            if (component_count > 0) {
                start = component_starts[--component_count];
                out_len = start > 1 ? start - 1 : 1;
                out[out_len] = '\0';
            }
            continue;
        }

        if (component_count >= ARRAY_LEN(component_starts))
            return 0;

        if (out_len > 1) {
            if (out_len + 1 >= out_cap)
                return 0;
            out[out_len++] = '/';
        }

        start = out_len;
        if (out_len + component_len >= out_cap)
            return 0;

        for (vk_usize i = 0; i < component_len; ++i)
            out[out_len++] = component[i];
        out[out_len] = '\0';
        component_starts[component_count++] = start;
    }

    return 1;
}

static int shell_resolve_path_from(const char* base,
                                   const char* raw,
                                   char* out,
                                   vk_usize out_cap)
{
    char combined[SHELL_PATH_MAX];
    const char* input = vk_skip_spaces(raw ? raw : "");

    if (*input == '\0') {
        shell_copy_line(out, out_cap, base);
        return 1;
    }

    if (shell_is_separator(*input))
        return shell_normalize_absolute_path(input, out, out_cap);

    vk_usize pos = 0;
    if (vk_strcmp(base, "/") == 0) {
        combined[pos++] = '/';
    } else {
        while (base[pos] != '\0') {
            if (pos + 1 >= sizeof(combined))
                return 0;
            combined[pos] = base[pos];
            ++pos;
        }
        if (pos + 1 >= sizeof(combined))
            return 0;
        combined[pos++] = '/';
    }

    while (*input != '\0') {
        if (pos + 1 >= sizeof(combined))
            return 0;
        combined[pos++] = *input++;
    }
    combined[pos] = '\0';

    return shell_normalize_absolute_path(combined, out, out_cap);
}

static int shell_resolve_path(const char* raw, char* out, vk_usize out_cap)
{
    return shell_resolve_path_from(s_cwd, raw, out, out_cap);
}

static void shell_query_default_path(char* out, vk_usize out_cap)
{
    char response[160];

    if (out == VK_NULL || out_cap == 0)
        return;

    out[0] = '\0';
    response[0] = '\0';

    vk_kobj_rpc_path_json("get", "fs/root_path", response, sizeof(response));
    if (vk_kobj_response_ok(response)
            && vk_json_extract_string_field(response, "value", out, out_cap)
            && out[0] != '\0') {
        return;
    }

    shell_copy_line(out, out_cap, "/");
}

static void shell_init_paths(void)
{
    shell_query_default_path(s_root_path, sizeof(s_root_path));
    shell_copy_line(s_cwd, sizeof(s_cwd), s_root_path);
}

static int shell_directory_exists(const char* path)
{
    char response[128];
    response[0] = '\0';
    vk_kobj_rpc_path_json("fs_list", path, response, sizeof(response));
    return vk_kobj_response_ok(response);
}

static int shell_parse_fs_item(const char* record,
                               int* is_directory,
                               char* name,
                               vk_usize name_cap,
                               vk_u64* size)
{
    const char* cursor;
    vk_u64 parsed_size = 0;
    vk_usize name_len = 0;

    if (record == VK_NULL || record[0] == '\0' || record[1] != '\t')
        return 0;

    if (is_directory != VK_NULL)
        *is_directory = record[0] == 'D';

    cursor = record + 2;
    while (*cursor != '\0' && *cursor != '\t') {
        if (name != VK_NULL && name_len + 1 < name_cap)
            name[name_len] = *cursor;
        ++name_len;
        ++cursor;
    }

    if (*cursor != '\t')
        return 0;

    if (name != VK_NULL && name_cap > 0) {
        vk_usize copy_len = name_len < name_cap - 1 ? name_len : name_cap - 1;
        name[copy_len] = '\0';
    }

    ++cursor;
    while (*cursor >= '0' && *cursor <= '9') {
        parsed_size = parsed_size * 10ULL + (vk_u64)(*cursor - '0');
        ++cursor;
    }

    if (size != VK_NULL)
        *size = parsed_size;

    return name_len > 0;
}

static void shell_print_ls_entry(const char* name, int is_directory, vk_u64 size)
{
    VK_CALL(puts, is_directory ? "d " : "f ");

    if (is_directory) {
        VK_CALL(puts, name);
        VK_CALL(puts, "/\n");
        return;
    }

    shell_put_padded(name, 28);
    VK_CALL(puts, "  ");
    shell_put_dec_width(size, 8);
    VK_CALL(putc, '\n');
}

static int shell_list_directory(const char* path)
{
    char response[SHELL_FS_RESPONSE_MAX];
    char raw_items[SHELL_FS_ITEMS_MAX][SHELL_FS_ITEM_MAX];
    int count;
    int printed = 0;

    VK_CALL(memset, response, 0, sizeof(response));
    VK_CALL(memset, raw_items, 0, sizeof(raw_items));
    vk_kobj_rpc_path_json("fs_list", path, response, sizeof(response));
    if (!vk_kobj_response_ok(response))
        return 0;

    count = vk_json_extract_string_array_field(response,
                                               "items",
                                               &raw_items[0][0],
                                               SHELL_FS_ITEM_MAX,
                                               SHELL_FS_ITEMS_MAX);

    for (int i = 0; i < count; ++i) {
        char name[SHELL_FS_ITEM_MAX];
        int is_directory = 0;
        vk_u64 size = 0;

        if (!shell_parse_fs_item(raw_items[i], &is_directory, name, sizeof(name), &size))
            continue;

        shell_print_ls_entry(name, is_directory, size);
        printed = 1;
    }

    if (!printed)
        VK_CALL(puts, "(empty)\n");

    return 1;
}

static void cmd_help(const char* arg)
{
    (void)arg;
    VK_CALL(puts, "Available commands:\n");
    VK_CALL(puts, "  help         - Show this message\n");
    VK_CALL(puts, "  version      - Show shell version\n");
    VK_CALL(puts, "  pwd          - Print current directory\n");
    VK_CALL(puts, "  cd [dir]     - Change current directory\n");
    VK_CALL(puts, "  ls [path]    - List files and directories\n");
    VK_CALL(puts, "  cat <file>   - Print a file\n");
    VK_CALL(puts, "  clear        - Clear the screen\n");
    VK_CALL(puts, "  reboot       - Reboot the machine\n");
    VK_CALL(puts, "  run <cmd>    - Launch a program with args\n");
    VK_CALL(puts, "  drvload <d>  - Load a driver (boot scripts use this)\n");
    VK_CALL(puts, "  exit         - Exit the shell\n");
    VK_CALL(puts, "Programs can also be launched directly: foo or foo.vbin\n");
}

static void cmd_version(const char* arg)
{
    (void)arg;
    VK_CALL(puts, "vkernel shell\n");
    VK_CALL(puts, "  API version: ");
    VK_CALL(put_dec, VK_API_VERSION);
    VK_CALL(puts, "\n");
}

static void cmd_pwd(const char* arg)
{
    (void)arg;
    VK_CALL(puts, s_cwd);
    VK_CALL(putc, '\n');
}

static void cmd_cd(const char* arg)
{
    char path[SHELL_PATH_MAX];
    const char* raw = vk_skip_spaces(arg);

    if (*raw == '\0')
        raw = s_root_path;

    if (!shell_resolve_path(raw, path, sizeof(path))) {
        VK_CALL(puts, "cd: path too long\n");
        return;
    }

    if (!shell_directory_exists(path)) {
        VK_CALL(puts, "cd: directory not found: ");
        VK_CALL(puts, raw);
        VK_CALL(putc, '\n');
        return;
    }
    shell_copy_line(s_cwd, sizeof(s_cwd), path);
}

static void cmd_ls(const char* arg)
{
    char path[SHELL_PATH_MAX];
    const char* raw = vk_skip_spaces(arg);

    if (!shell_resolve_path(raw, path, sizeof(path))) {
        VK_CALL(puts, "ls: path too long\n");
        return;
    }

    if (shell_list_directory(path))
        return;

    if (VK_CALL(file_exists, path)) {
        shell_print_ls_entry(shell_basename(path), 0, VK_CALL(file_size, path));
        return;
    }

    VK_CALL(puts, "ls: not found: ");
    VK_CALL(puts, raw[0] != '\0' ? raw : path);
    VK_CALL(putc, '\n');
}

static void cmd_cat(const char* arg)
{
    char path[SHELL_PATH_MAX];
    const char* raw = vk_skip_spaces(arg);

    if (*raw == '\0') {
        VK_CALL(puts, "Usage: cat <filename>\n");
        return;
    }

    if (!shell_resolve_path(raw, path, sizeof(path))) {
        VK_CALL(puts, "cat: path too long\n");
        return;
    }

    vk_file_handle_t fh = VK_CALL(file_open, path, "r");
    if (fh == (vk_file_handle_t)0) {
        VK_CALL(puts, "cat: file not found: ");
        VK_CALL(puts, raw);
        VK_CALL(puts, "\n");
        return;
    }

    unsigned char buf[128];
    vk_usize n;
    while ((n = VK_CALL(file_read_handle, fh, buf, sizeof(buf))) > 0) {
        for (vk_usize i = 0; i < n; ++i)
            VK_CALL(putc, (char)buf[i]);
    }

    VK_CALL(file_close, fh);
    VK_CALL(puts, "\n");
}

static void cmd_clear(const char* arg)
{
    (void)arg;
    VK_CALL(clear);
}

static void cmd_reboot(const char* arg)
{
    (void)arg;
    vk_kobj_cmd_json("reboot");
}

static int shell_extract_program_path(const char* command_line,
                                      char* out,
                                      vk_usize out_cap,
                                      const char** out_rest)
{
    const char* p = vk_skip_spaces(command_line);
    vk_usize pos = 0;
    char quote = 0;

    if (out_rest)
        *out_rest = p;

    if (*p == '\0' || out_cap < 2)
        return 0;

    if (*p == '"' || *p == '\'')
        quote = *p++;

    while (*p != '\0') {
        char ch = *p;
        if (quote) {
            if (ch == quote) {
                ++p;
                break;
            }
        } else if (ch == ' ' || ch == '\t') {
            break;
        }

        if (pos + 1 >= out_cap)
            return 0;
        out[pos++] = ch;
        ++p;
    }

    out[pos] = '\0';
    if (pos == 0)
        return 0;

    p = vk_skip_spaces(p);
    if (out_rest)
        *out_rest = p;

    return 1;
}

static int shell_append_string(char* out,
                               vk_usize out_cap,
                               vk_usize* pos,
                               const char* text)
{
    while (*text != '\0') {
        if (*pos + 1 >= out_cap)
            return 0;
        out[(*pos)++] = *text++;
    }

    out[*pos] = '\0';
    return 1;
}

static int shell_try_resolve_program_path(const char* raw_path,
                                          char* out,
                                          vk_usize out_cap)
{
    char path[SHELL_PATH_MAX];
    vk_usize len;

    if (!shell_resolve_path(raw_path, path, sizeof(path)))
        return 0;

    if (VK_CALL(file_exists, path)) {
        shell_copy_line(out, out_cap, path);
        return 1;
    }

    if (vk_has_suffix(path, ".vbin"))
        return 0;

    len = vk_strlen(path);
    if (len + 5 >= sizeof(path))
        return 0;

    path[len + 0] = '.';
    path[len + 1] = 'v';
    path[len + 2] = 'b';
    path[len + 3] = 'i';
    path[len + 4] = 'n';
    path[len + 5] = '\0';

    if (!VK_CALL(file_exists, path))
        return 0;

    shell_copy_line(out, out_cap, path);
    return 1;
}

static int shell_build_resolved_command_line(const char* path,
                                             const char* rest,
                                             char* out,
                                             vk_usize out_cap)
{
    vk_usize pos = 0;

    if (out_cap == 0)
        return 0;

    out[0] = '\0';
    if (shell_path_has_spaces(path)) {
        if (pos + 1 >= out_cap)
            return 0;
        out[pos++] = '"';
        out[pos] = '\0';
        if (!shell_append_string(out, out_cap, &pos, path))
            return 0;
        if (pos + 1 >= out_cap)
            return 0;
        out[pos++] = '"';
        out[pos] = '\0';
    } else if (!shell_append_string(out, out_cap, &pos, path)) {
        return 0;
    }

    if (rest != VK_NULL && *rest != '\0') {
        if (pos + 1 >= out_cap)
            return 0;
        out[pos++] = ' ';
        out[pos] = '\0';
        if (!shell_append_string(out, out_cap, &pos, rest))
            return 0;
    }

    return 1;
}

static int shell_launch_program(const char* command_line, int verbose)
{
    const char* rest = VK_NULL;
    char program_path[SHELL_PATH_MAX];
    char resolved_path[SHELL_PATH_MAX];
    char resolved_cmdline[SHELL_LINE_MAX];
    const char* cmd = vk_skip_spaces(command_line);
    int has_extra_args = 0;
    vk_i64 task_id;

    if (*cmd == '\0') {
        if (verbose)
            VK_CALL(puts, "Usage: run <program> [args...]\n");
        return -1;
    }

    if (!shell_extract_program_path(cmd, program_path, sizeof(program_path), &rest)) {
        if (verbose)
            VK_CALL(puts, "Usage: run <program> [args...]\n");
        return -1;
    }

    has_extra_args = rest != VK_NULL && *rest != '\0';
    if (!shell_try_resolve_program_path(program_path, resolved_path, sizeof(resolved_path))) {
        if (verbose) {
            VK_CALL(puts, "run: program not found: ");
            VK_CALL(puts, program_path);
            VK_CALL(putc, '\n');
        }
        return -1;
    }

    if (!shell_build_resolved_command_line(resolved_path, rest, resolved_cmdline, sizeof(resolved_cmdline))) {
        if (verbose)
            VK_CALL(puts, "run: command line too long\n");
        return -1;
    }

    task_id = vk_get_api()->vk_run_cmdline(resolved_cmdline);

    if (task_id < 0) {
        if (verbose) {
            VK_CALL(puts, "run: failed to launch ");
            VK_CALL(puts, resolved_path);
            VK_CALL(putc, '\n');
        }
        return -1;
    }

    if (verbose) {
        VK_CALL(puts, "run: spawned task ");
        VK_CALL(put_dec, (vk_u64)task_id);
        VK_CALL(puts, "\n");
    }

    VK_CALL(wait_task, task_id);
    return 0;
}

static void cmd_run(const char* arg)
{
    (void)shell_launch_program(arg, 1);
}

static void cmd_drvload(const char* arg)
{
    const char* name = vk_skip_spaces(arg);
    if (*name == '\0') {
        VK_CALL(puts, "Usage: drvload <driver_name>\n");
        VK_CALL(puts, "Example: drvload sb16.vko\n");
        return;
    }
    vk_kobj_named_cmd_json("drvload", name);
}

static void cmd_exit(const char* arg)
{
    (void)arg;
    VK_CALL(exit, 0);
}

/* -------------------------------------------------------------------------
 * Command dispatch
 *
 * Two separate, homogeneous tables — one for exact matches, one for prefix
 * commands. Keeping the tables homogeneous (every field always valid)
 * avoids any compiler trouble with partial zero-initialization of mixed
 * pointer/non-pointer struct fields in freestanding mode.
 * ---------------------------------------------------------------------- */

typedef void (*cmd_fn)(const char*);

typedef struct {
    const char* keyword;
    cmd_fn      fn;
} exact_cmd_t;

typedef struct {
    const char* prefix; /* includes trailing space, e.g. "cat " */
    vk_usize    skip;   /* bytes past the prefix to pass as arg  */
    cmd_fn      fn;
} prefix_cmd_t;

static const exact_cmd_t EXACT_CMDS[] = {
    { "help",    cmd_help     },
    { "?",       cmd_help     },
    { "version", cmd_version  },
    { "pwd",     cmd_pwd      },
    { "cd",      cmd_cd       },
    { "ls",      cmd_ls       },
    { "cat",     cmd_cat      },
    { "run",     cmd_run      },
    { "drvload", cmd_drvload  },
    { "clear",   cmd_clear    },
    { "reboot",  cmd_reboot   },
    { "exit",    cmd_exit     },
};

static const prefix_cmd_t PREFIX_CMDS[] = {
    { "cat ",       4,  cmd_cat       },
    { "cd ",        3,  cmd_cd        },
    { "ls ",        3,  cmd_ls        },
    { "run ",       4,  cmd_run       },
    { "drvload ",   8,  cmd_drvload   },
};

static int parse_cmdline(const char* cmdline)
{
    vk_usize i;

    for (i = 0; i < ARRAY_LEN(PREFIX_CMDS); ++i) {
        if (vk_has_prefix(cmdline, PREFIX_CMDS[i].prefix)) {
            PREFIX_CMDS[i].fn(cmdline + PREFIX_CMDS[i].skip);
            return 0;
        }
    }

    for (i = 0; i < ARRAY_LEN(EXACT_CMDS); ++i) {
        if (vk_strcmp(cmdline, EXACT_CMDS[i].keyword) == 0) {
            EXACT_CMDS[i].fn("");
            return 0;
        }
    }

    if (shell_launch_program(cmdline, 0) == 0)
        return 0;

    VK_CALL(puts, "Command not found: ");
    VK_CALL(puts, cmdline);
    VK_CALL(puts, "\n");
    VK_CALL(puts, "Type 'help' for available commands.\n");
    return -1;
}

/* -------------------------------------------------------------------------
 * Startup script
 * ---------------------------------------------------------------------- */

/*
 * Read and execute "shell.txt" line by line using a byte accumulator.
 * Handles files that don't end with a trailing newline.
 */
static void read_startup_script(void)
{
    vk_file_handle_t fh = VK_CALL(file_open, "shell.txt", "r");
    if (fh == (vk_file_handle_t)0) {
        VK_CALL(puts, "No startup script found (shell.txt), skipping...\n");
        return;
    }

    char     chunk[256];
    char     line[256];
    vk_usize line_pos = 0;
    vk_usize n;
    vk_usize i;

    while ((n = VK_CALL(file_read_handle, fh, chunk, sizeof(chunk))) > 0) {
        for (i = 0; i < n; ++i) {
            char c = chunk[i];

            if (c == '\n') {
                line[line_pos] = '\0';
                line_pos = 0;

                const char* cmd = vk_skip_spaces(line);
                if (*cmd == '#' || *cmd == '\0')
                    continue;

                VK_CALL(puts, SHELL_PROMPT);
                VK_CALL(puts, cmd);
                VK_CALL(puts, "\n");
                parse_cmdline(cmd);
                continue;
            }

            /* Silently truncate lines that exceed the buffer. */
            if (line_pos < sizeof(line) - 1)
                line[line_pos++] = c;
        }
    }

    /* Handle a final line with no trailing newline. */
    if (line_pos > 0) {
        line[line_pos] = '\0';
        const char* cmd = vk_skip_spaces(line);
        if (*cmd != '#' && *cmd != '\0') {
            VK_CALL(puts, SHELL_PROMPT);
            VK_CALL(puts, cmd);
            VK_CALL(puts, "\n");
            parse_cmdline(cmd);
        }
    }

    VK_CALL(file_close, fh);
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int _start(const vk_api_t* api)
{
    vk_init(api);
    shell_init_paths();

    int run_startup_script = 0;

    VK_CALL(puts, "\n\n");
    VK_CALL(puts, "+----------------------------------+\n");
    VK_CALL(puts, "|     vkernel userspace shell      |\n");
    VK_CALL(puts, "+----------------------------------+\n");
    VK_CALL(puts, "Type 'help' for available commands.\n\n");

    if (vk_get_api()->vk_get_cmdline) {
        run_startup_script = shell_cmdline_has_flag("--startup") ? 1 : 0;
    } else {
        /* Compatibility path for older kernels that do not expose cmdline. */
        run_startup_script = shell_has_framebuffer();
    }

    if (run_startup_script && shell_has_framebuffer())
        read_startup_script();

    char line[256];
    for (;;) {
        VK_CALL(puts, SHELL_PROMPT);
        vk_usize len = console_getline(line, sizeof(line), SHELL_PROMPT);
        if (len == 0)
            continue;

        const char* cmd = vk_skip_spaces(line);
        if (*cmd == '\0')
            continue;

        parse_cmdline(cmd);
    }

    return 0;
}
