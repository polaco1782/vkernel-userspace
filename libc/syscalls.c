/*
 * vkernel userspace - newlib system call stubs
 * Copyright (C) 2026 vkernel authors
 *
 * syscalls.c - Implements the POSIX-like system calls that newlib's
 *              libc.a expects.  Each stub translates to the vkernel
 *              kernel API via vk_get_api().
 *
 * Compile with newlib headers (-isystem sysroot/include).
 */

/*
 * For the x86_64-elf target newlib maps the underscore syscall names
 * (e.g. _close) to their non-underscore equivalents (close) via the
 * MISSING_SYSCALL_NAMES macro in <_syslist.h>.  Define it here so that
 * our own function definitions are renamed in the same way and the
 * linker finds them.
 */
#define MISSING_SYSCALL_NAMES
#include <_syslist.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <reent.h>
#include <stdarg.h>

#include "../include/vk.h"

/* ============================================================
 * File-descriptor table
 *
 * newlib's stdio operates on integer file descriptors.  The vkernel
 * kernel uses opaque vk_file_handle_t (u64) handles.  We maintain a
 * small mapping table.  Descriptors 0-2 are reserved for stdin,
 * stdout, stderr and are handled specially (console I/O).
 * ============================================================ */

#define VK_MAX_FDS   64
#define VK_FD_STDIN   0
#define VK_FD_STDOUT  1
#define VK_FD_STDERR  2

typedef struct {
    vk_file_handle_t handle;
    vk_u64           inode;
    int              in_use;
} vk_fd_entry_t;

static vk_fd_entry_t _fd_table[VK_MAX_FDS] = {
    [VK_FD_STDIN]  = { 0, 1 },
    [VK_FD_STDOUT] = { 0, 1 },
    [VK_FD_STDERR] = { 0, 1 },
};

static int _fd_alloc(vk_file_handle_t h)
{
    for (int i = 3; i < VK_MAX_FDS; ++i) {
        if (!_fd_table[i].in_use) {
            _fd_table[i].handle = h;
            _fd_table[i].inode = 0;
            _fd_table[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

static void _fd_free(int fd)
{
    if (fd >= 3 && fd < VK_MAX_FDS) {
        _fd_table[fd].in_use = 0;
        _fd_table[fd].handle = 0;
        _fd_table[fd].inode = 0;
    }
}

static int _fd_valid(int fd)
{
    return fd >= 0 && fd < VK_MAX_FDS && _fd_table[fd].in_use;
}

static vk_u64 _path_inode(const char* path)
{
    const unsigned char* cur = (const unsigned char*)path;
    vk_u64 hash = 1469598103934665603ull;

    if (!cur) {
        return 1;
    }

    while (*cur != '\0') {
        hash ^= (vk_u64)(*cur++);
        hash *= 1099511628211ull;
    }

    return hash != 0 ? hash : 1;
}

static int _fd_size(int fd, off_t* out_size)
{
    if (!_fd_valid(fd) || out_size == 0) {
        errno = out_size == 0 ? EFAULT : EBADF;
        return -1;
    }

    const vk_file_handle_t handle = _fd_table[fd].handle;
    const vk_i64 original = VK_CALL(file_tell, handle);
    if (original < 0) {
        errno = EIO;
        return -1;
    }
    if (VK_CALL(file_seek, handle, 0, 2) != 0) {
        errno = EIO;
        return -1;
    }

    const vk_i64 end = VK_CALL(file_tell, handle);
    const int restore_rc = VK_CALL(file_seek, handle, original, 0);
    if (end < 0 || restore_rc != 0) {
        errno = EIO;
        return -1;
    }

    *out_size = (off_t)end;
    return 0;
}

/* ============================================================
 * Heap — _sbrk
 *
 * We request one arena from the kernel's malloc and hand it out linearly.
 * Newlib malloc requires _sbrk to return one monotonic address space, so a
 * non-contiguous growth allocation must fail instead of switching arenas.
 * ============================================================ */

#define VK_HEAP_INITIAL_SIZE  (32 * 1024 * 1024)  /* 32 MiB */
#define VK_HEAP_GROW_SIZE     (4 * 1024 * 1024)   /* 4 MiB increments */

static char* _heap_start = 0;
static char* _heap_ptr   = 0;
static char* _heap_end   = 0;

void* _sbrk(ptrdiff_t incr)
{
    if (incr < 0) {
        if (_heap_start == 0 || _heap_ptr + incr < _heap_start) {
            errno = EINVAL;
            return (void*)-1;
        }

        char* prev = _heap_ptr;
        _heap_ptr += incr;
        return prev;
    }

    /* First call — allocate the initial arena. */
    if (_heap_start == 0) {
        _heap_start = (char*)VK_CALL(malloc, VK_HEAP_INITIAL_SIZE);
        if (!_heap_start) {
            errno = ENOMEM;
            return (void*)-1;
        }
        _heap_ptr = _heap_start;
        _heap_end = _heap_start + VK_HEAP_INITIAL_SIZE;
    }

    /* Can we satisfy from the current arena? */
    if (_heap_ptr + incr <= _heap_end) {
        char* prev = _heap_ptr;
        _heap_ptr += incr;
        return prev;
    }

    /* Try to grow.  Allocate a new, larger chunk and hope it is
       contiguous (the kernel heap usually is for small requests). */
    vk_usize grow = (vk_usize)incr > VK_HEAP_GROW_SIZE
                        ? (vk_usize)incr
                        : VK_HEAP_GROW_SIZE;
    char* extra = (char*)VK_CALL(malloc, grow);
    if (!extra) {
        errno = ENOMEM;
        return (void*)-1;
    }

    /* If the new block happens to be contiguous, extend the arena. */
    if (extra == _heap_end) {
        _heap_end += grow;
        char* prev = _heap_ptr;
        _heap_ptr += incr;
        return prev;
    }

    /* Newlib malloc assumes sbrk is a single monotonic address space.
       Returning a non-contiguous arena corrupts malloc's view of the heap. */
    VK_CALL(free, extra);
    errno = ENOMEM;
    return (void*)-1;
}

/* ============================================================
 * Process control
 * ============================================================ */

void _exit(int code)
{
    VK_CALL(exit, code);
    __builtin_unreachable();
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = ESRCH;
    return -1;
}

int _getpid(void)
{
    return 1;
}

/* ============================================================
 * File I/O
 * ============================================================ */

int _open(const char* path, int flags, ...)
{
    if (!path) {
        errno = EFAULT;
        return -1;
    }

    /*
     * Translate POSIX flags to a simple mode string.
     * newlib's _open uses O_RDONLY=0, O_WRONLY=1, O_RDWR=2, O_CREAT etc.
     * We map conservatively; the kernel side is simple enough.
     */
    const char* vk_mode = "r";
    const int accmode = flags & O_ACCMODE;
    const int wants_create = (flags & O_CREAT) != 0;
    const int wants_truncate = (flags & O_TRUNC) != 0;
    const int wants_append = (flags & O_APPEND) != 0;

    if (wants_append) {
        vk_mode = accmode == O_RDWR ? "a+" : "a";
    } else if (wants_truncate) {
        vk_mode = accmode == O_RDWR ? "w+" : "w";
    } else if (wants_create && accmode == O_RDWR) {
        /* SQLite opens databases as O_RDWR|O_CREAT but only wants a truncate
           on first creation. Preserve existing contents when the file exists. */
        vk_mode = VK_CALL(file_exists, path) ? "r+" : "w+";
    } else if (accmode == O_WRONLY) {
        vk_mode = "w";
    } else if (accmode == O_RDWR) {
        vk_mode = "r+";
    }

    vk_file_handle_t h = VK_CALL(file_open, path, vk_mode);
    if (h == 0) {
        errno = ENOENT;
        return -1;
    }

    int fd = _fd_alloc(h);
    if (fd < 0) {
        VK_CALL(file_close, h);
        errno = EMFILE;
        return -1;
    }

    _fd_table[fd].inode = _path_inode(path);

    return fd;
}

int _close(int fd)
{
    if (fd <= VK_FD_STDERR) return 0;   /* never close std streams */

    if (!_fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }

    int rc = VK_CALL(file_close, _fd_table[fd].handle);
    _fd_free(fd);
    return rc;
}

int _read(int fd, char* buf, int len)
{
    if (len <= 0) return 0;

    /* stdin — character-at-a-time from console */
    if (fd == VK_FD_STDIN) {
        for (int i = 0; i < len; ++i) {
            char c = VK_CALL(getc);
            buf[i] = c;
            if (c == '\n' || c == '\r')
                return i + 1;
        }
        return len;
    }

    if (!_fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }

    vk_usize n = VK_CALL(file_read_handle, _fd_table[fd].handle, buf, (vk_usize)len);
    return (int)n;
}

int _write(int fd, const char* buf, int len)
{
    if (len <= 0) return 0;

    /* stdout / stderr → console */
    if (fd == VK_FD_STDOUT || fd == VK_FD_STDERR) {
        if (vk_get_api()->vk_stdio_write) {
            return (int)vk_get_api()->vk_stdio_write(buf, (vk_usize)len);
        }

        for (int i = 0; i < len; ++i) {
            VK_CALL(putc, buf[i]);
        }
        return len;
    }

    if (!_fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }

    vk_usize n = VK_CALL(file_write_handle, _fd_table[fd].handle, buf, (vk_usize)len);
    return (int)n;
}

int _lseek(int fd, int offset, int whence)
{
    if (fd <= VK_FD_STDERR) {
        errno = ESPIPE;
        return -1;
    }

    if (!_fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }

    int rc = VK_CALL(file_seek, _fd_table[fd].handle, (vk_i64)offset, whence);
    if (rc != 0) {
        errno = EINVAL;
        return -1;
    }

    vk_i64 pos = VK_CALL(file_tell, _fd_table[fd].handle);
    return (int)pos;
}

int _fstat(int fd, struct stat* st)
{
    if (!st) {
        errno = EFAULT;
        return -1;
    }

    *st = (struct stat){0};

    if (fd <= VK_FD_STDERR) {
        st->st_mode = S_IFCHR;
        return 0;
    }

    if (!_fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }

    if (_fd_size(fd, &st->st_size) != 0) {
        return -1;
    }

    st->st_mode = S_IFREG | 0666;
    st->st_nlink = 1;
    st->st_blksize = 512;
    st->st_dev = 1;
    st->st_ino = (ino_t)_fd_table[fd].inode;
    return 0;
}

int _stat(const char* path, struct stat* st)
{
    if (!path || !st) {
        errno = EFAULT;
        return -1;
    }

    *st = (struct stat){0};

    if (VK_CALL(file_exists, path)) {
        st->st_mode = S_IFREG | 0666;
        st->st_size = (off_t)VK_CALL(file_size, path);
        st->st_nlink = 1;
        st->st_blksize = 512;
        st->st_dev = 1;
        st->st_ino = (ino_t)_path_inode(path);
        return 0;
    }

    errno = ENOENT;
    return -1;
}

int _link(const char* old, const char* new_path)
{
    (void)old;
    (void)new_path;
    errno = EMLINK;
    return -1;
}

int _unlink(const char* path)
{
    if (!path) {
        errno = EFAULT;
        return -1;
    }

    return VK_CALL(file_remove, path);
}

int _isatty(int fd)
{
    return (fd <= VK_FD_STDERR) ? 1 : 0;
}

/* ============================================================
 * Time (stub — no RTC yet)
 * ============================================================ */

#include <sys/times.h>

int _gettimeofday(struct timeval* tv, void* tz)
{
    (void)tz;
    if (tv) {
        vk_u64 ticks = VK_CALL(tick_count);
        vk_u32 tps   = VK_CALL(ticks_per_sec);
        tv->tv_sec  = (time_t)(ticks / tps);
        tv->tv_usec = (suseconds_t)((ticks % tps) * 1000000 / tps);
    }
    return 0;
}

clock_t _times(struct tms* buf)
{
    vk_u64 ticks = VK_CALL(tick_count);
    if (buf) {
        buf->tms_utime  = (clock_t)ticks;
        buf->tms_stime  = 0;
        buf->tms_cutime = 0;
        buf->tms_cstime = 0;
    }
    return (clock_t)ticks;
}

int access(const char* path, int mode)
{
    (void)mode;

    struct stat st = {0};
    if (stat(path, &st) == 0) {
        return 0;
    }

    errno = ENOENT;
    return -1;
}

int fsync(int fd)
{
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }

    /* The kernel file API is synchronous today, so there is nothing extra to flush. */
    return 0;
}

int ftruncate(int fd, off_t length)
{
    if (!_fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }

    if (VK_CALL(file_truncate, _fd_table[fd].handle, (vk_i64)length) != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int fchmod(int fd, mode_t mode)
{
    (void)fd;
    (void)mode;
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group)
{
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

int mkdir(const char* path, mode_t mode)
{
    (void)path;
    (void)mode;
    errno = ENOSYS;
    return -1;
}

int rmdir(const char* path)
{
    (void)path;
    errno = ENOSYS;
    return -1;
}

uid_t geteuid(void)
{
    return 0;
}

char* getcwd(char* buf, size_t size)
{
    if (buf == 0 || size < 2) {
        errno = ERANGE;
        return 0;
    }

    buf[0] = '/';
    buf[1] = '\0';
    return buf;
}

ssize_t pread(int fd, void* buf, size_t nbytes, off_t offset)
{
    const int original = lseek(fd, 0, 1);
    if (original < 0) {
        return -1;
    }
    if (lseek(fd, (int)offset, 0) < 0) {
        return -1;
    }

    const int read_count = read(fd, buf, (int)nbytes);
    (void)lseek(fd, original, 0);
    return read_count;
}

ssize_t pwrite(int fd, const void* buf, size_t nbytes, off_t offset)
{
    const int original = lseek(fd, 0, 1);
    if (original < 0) {
        return -1;
    }
    if (lseek(fd, (int)offset, 0) < 0) {
        return -1;
    }

    const int write_count = write(fd, buf, (int)nbytes);
    (void)lseek(fd, original, 0);
    return write_count;
}

int fcntl(int fd, int cmd, ...)
{
    (void)fd;

    va_list args;
    va_start(args, cmd);

    if (cmd == F_GETLK) {
        struct flock* lock = va_arg(args, struct flock*);
        if (lock != 0) {
            lock->l_type = F_UNLCK;
        }
        va_end(args);
        return 0;
    }

    va_end(args);

    if (cmd == F_GETFL) {
        return O_RDWR;
    }

    return 0;
}

ssize_t readlink(const char* path, char* buf, size_t bufsize)
{
    (void)path;
    (void)buf;
    (void)bufsize;
    errno = EINVAL;
    return -1;
}

unsigned sleep(unsigned seconds)
{
    if (seconds == 0 || vk_get_api()->vk_sleep == 0 || vk_get_api()->vk_ticks_per_sec == 0) {
        return 0;
    }

    const vk_u64 ticks = (vk_u64)seconds * vk_get_api()->vk_ticks_per_sec();
    vk_get_api()->vk_sleep(ticks);
    return 0;
}

int usleep(useconds_t usec)
{
    if (usec == 0 || vk_get_api()->vk_sleep == 0 || vk_get_api()->vk_ticks_per_sec == 0) {
        return 0;
    }

    const vk_u64 ticks_per_second = vk_get_api()->vk_ticks_per_sec();
    vk_u64 ticks = ((vk_u64)usec * ticks_per_second + 999999ULL) / 1000000ULL;
    if (ticks == 0) {
        ticks = 1;
    }

    vk_get_api()->vk_sleep(ticks);
    return 0;
}

int utimes(const char* path, const struct timeval times[2])
{
    (void)path;
    (void)times;
    return 0;
}

/* ============================================================
 * Misc stubs
 * ============================================================ */

int _fork(void)
{
    errno = ENOSYS;
    return -1;
}

int _wait(int* status)
{
    (void)status;
    errno = ECHILD;
    return -1;
}

#define VK_EXEC_CMDLINE_MAX 256

static int _exec_arg_needs_quotes(const char* arg)
{
    if (!arg || *arg == '\0') {
        return 1;
    }

    while (*arg != '\0') {
        if (*arg == ' ' || *arg == '\t' || *arg == '\n' || *arg == '\r'
            || *arg == '"' || *arg == '\'') {
            return 1;
        }
        ++arg;
    }

    return 0;
}

static char _exec_quote_char(const char* arg)
{
    int has_single_quote = 0;
    int has_double_quote = 0;

    while (arg && *arg != '\0') {
        if (*arg == '\'') {
            has_single_quote = 1;
        } else if (*arg == '"') {
            has_double_quote = 1;
        }
        ++arg;
    }

    if (has_double_quote && !has_single_quote) {
        return '\'';
    }

    return '"';
}

static int _exec_append_char(char* out, vk_usize out_cap, vk_usize* len, char ch)
{
    if (!out || !len || *len + 1 >= out_cap) {
        return 0;
    }

    out[*len] = ch;
    ++(*len);
    out[*len] = '\0';
    return 1;
}

static int _exec_append_arg(char* out, vk_usize out_cap, vk_usize* len, const char* arg)
{
    if (!out || !len || !arg) {
        return 0;
    }

    if (*len != 0 && !_exec_append_char(out, out_cap, len, ' ')) {
        return 0;
    }

    if (!_exec_arg_needs_quotes(arg)) {
        while (*arg != '\0') {
            if (!_exec_append_char(out, out_cap, len, *arg++)) {
                return 0;
            }
        }
        return 1;
    }

    const char quote = _exec_quote_char(arg);
    if (!_exec_append_char(out, out_cap, len, quote)) {
        return 0;
    }

    while (*arg != '\0') {
        if (*arg == quote) {
            if (!_exec_append_char(out, out_cap, len, '\\')) {
                return 0;
            }
        }
        if (!_exec_append_char(out, out_cap, len, *arg++)) {
            return 0;
        }
    }

    return _exec_append_char(out, out_cap, len, quote);
}

int _execve(const char* name, char* const argv[], char* const env[])
{
    (void)env;

    if (!name) {
        errno = EFAULT;
        return -1;
    }

    if (!VK_CALL(file_exists, name)) {
        errno = ENOENT;
        return -1;
    }

    if (!vk_get_api()->vk_exec_cmdline) {
        errno = ENOSYS;
        return -1;
    }

    char cmdline[VK_EXEC_CMDLINE_MAX] = {0};
    vk_usize len = 0;
    if (!_exec_append_arg(cmdline, sizeof(cmdline), &len, name)) {
        errno = E2BIG;
        return -1;
    }

    if (argv && argv[0]) {
        for (int index = 1; argv[index] != 0; ++index) {
            if (!_exec_append_arg(cmdline, sizeof(cmdline), &len, argv[index])) {
                errno = E2BIG;
                return -1;
            }
        }
    }

    if (vk_get_api()->vk_exec_cmdline(cmdline) < 0) {
        errno = ENOEXEC;
        return -1;
    }

    errno = ENOEXEC;
    return -1;
}
