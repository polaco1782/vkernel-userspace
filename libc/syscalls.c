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
#include <time.h>

#include "../include/vk.h"

#ifndef PROT_NONE
#define PROT_NONE 0
#endif
#ifndef PROT_READ
#define PROT_READ 0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif
#ifndef PROT_EXEC
#define PROT_EXEC 0x4
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif
#ifndef MAP_FIXED
#define MAP_FIXED 0x10
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

static int _vk_errno_to_host(int err)
{
    switch (err) {
        case VK_ERR_PERM: return EPERM;
        case VK_ERR_NOENT: return ENOENT;
        case VK_ERR_SRCH: return ESRCH;
        case VK_ERR_INTR: return EINTR;
        case VK_ERR_IO: return EIO;
        case VK_ERR_NXIO: return ENXIO;
        case VK_ERR_2BIG: return E2BIG;
        case VK_ERR_NOEXEC: return ENOEXEC;
        case VK_ERR_BADF: return EBADF;
        case VK_ERR_CHILD: return ECHILD;
        case VK_ERR_AGAIN: return EAGAIN;
        case VK_ERR_NOMEM: return ENOMEM;
        case VK_ERR_ACCES: return EACCES;
        case VK_ERR_FAULT: return EFAULT;
        case VK_ERR_BUSY: return EBUSY;
        case VK_ERR_EXIST: return EEXIST;
        case VK_ERR_XDEV: return EXDEV;
        case VK_ERR_NODEV: return ENODEV;
        case VK_ERR_NOTDIR: return ENOTDIR;
        case VK_ERR_ISDIR: return EISDIR;
        case VK_ERR_INVAL: return EINVAL;
        case VK_ERR_NFILE: return ENFILE;
        case VK_ERR_MFILE: return EMFILE;
        case VK_ERR_NOTTY: return ENOTTY;
        case VK_ERR_FBIG: return EFBIG;
        case VK_ERR_NOSPC: return ENOSPC;
        case VK_ERR_SPIPE: return ESPIPE;
        case VK_ERR_ROFS: return EROFS;
        case VK_ERR_MLINK: return EMLINK;
        case VK_ERR_PIPE: return EPIPE;
        case VK_ERR_RANGE: return ERANGE;
        case VK_ERR_NOSYS: return ENOSYS;
        case VK_ERR_NOTEMPTY: return ENOTEMPTY;
        case VK_ERR_LOOP: return ELOOP;
#ifdef ENODATA
        case VK_ERR_NODATA: return ENODATA;
#else
        case VK_ERR_NODATA: return ENOENT;
#endif
        default: return EIO;
    }
}

static int _vk_ret(vk_i64 ret)
{
    if (ret >= 0) {
        return (int)ret;
    }

    errno = _vk_errno_to_host((int)(-ret));
    return -1;
}

static ssize_t _vk_ret_ssize(vk_i64 ret)
{
    if (ret >= 0) {
        return (ssize_t)ret;
    }

    errno = _vk_errno_to_host((int)(-ret));
    return -1;
}

static int _translate_open_flags(int flags)
{
    int vk_flags = 0;

    switch (flags & O_ACCMODE) {
        case O_WRONLY:
            vk_flags |= VK_O_WRONLY;
            break;
        case O_RDWR:
            vk_flags |= VK_O_RDWR;
            break;
        case O_RDONLY:
        default:
            vk_flags |= VK_O_RDONLY;
            break;
    }

    if ((flags & O_CREAT) != 0) {
        vk_flags |= VK_O_CREAT;
    }
    if ((flags & O_TRUNC) != 0) {
        vk_flags |= VK_O_TRUNC;
    }
    if ((flags & O_APPEND) != 0) {
        vk_flags |= VK_O_APPEND;
    }

    return vk_flags;
}

static int _translate_seek_whence(int whence)
{
    switch (whence) {
        case SEEK_SET: return VK_SEEK_SET;
        case SEEK_CUR: return VK_SEEK_CUR;
        case SEEK_END: return VK_SEEK_END;
        default: return -1;
    }
}

static int _translate_fcntl_cmd(int cmd)
{
    switch (cmd) {
        case F_GETFD: return VK_F_GETFD;
        case F_SETFD: return VK_F_SETFD;
        case F_GETFL: return VK_F_GETFL;
        case F_SETFL: return VK_F_SETFL;
        case F_GETLK: return VK_F_GETLK;
        case F_SETLK: return VK_F_SETLK;
        case F_SETLKW: return VK_F_SETLKW;
        default: return -1;
    }
}

static int _translate_open_flags_back(int vk_flags)
{
    int flags = 0;

    switch (vk_flags & VK_O_ACCMODE) {
        case VK_O_WRONLY:
            flags |= O_WRONLY;
            break;
        case VK_O_RDWR:
            flags |= O_RDWR;
            break;
        case VK_O_RDONLY:
        default:
            flags |= O_RDONLY;
            break;
    }

    if ((vk_flags & VK_O_CREAT) != 0) {
        flags |= O_CREAT;
    }
    if ((vk_flags & VK_O_TRUNC) != 0) {
        flags |= O_TRUNC;
    }
    if ((vk_flags & VK_O_APPEND) != 0) {
        flags |= O_APPEND;
    }

    return flags;
}

static int _translate_prot_flags(int prot)
{
    int vk_prot = 0;
    if ((prot & PROT_READ) != 0) {
        vk_prot |= VK_PROT_READ;
    }
    if ((prot & PROT_WRITE) != 0) {
        vk_prot |= VK_PROT_WRITE;
    }
    if ((prot & PROT_EXEC) != 0) {
        vk_prot |= VK_PROT_EXEC;
    }
    return vk_prot;
}

static int _translate_mmap_flags(int flags)
{
    int vk_flags = 0;
    if ((flags & MAP_PRIVATE) != 0) {
        vk_flags |= VK_MAP_PRIVATE;
    }
#ifdef MAP_FIXED
    if ((flags & MAP_FIXED) != 0) {
        vk_flags |= VK_MAP_FIXED;
    }
#endif
#ifdef MAP_ANONYMOUS
    if ((flags & MAP_ANONYMOUS) != 0) {
        vk_flags |= VK_MAP_ANONYMOUS;
    }
#elif defined(MAP_ANON)
    if ((flags & MAP_ANON) != 0) {
        vk_flags |= VK_MAP_ANONYMOUS;
    }
#endif
    return vk_flags;
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
    return _vk_ret(vk_syscall(VK_SYS_GETPID, 0, 0, 0, 0, 0, 0));
}

/* ============================================================
 * File I/O
 * ============================================================ */

int _open(const char* path, int flags, ...)
{
    const int vk_flags = _translate_open_flags(flags);
    return _vk_ret(vk_syscall(VK_SYS_OPEN,
                              (vk_u64)(vk_usize)path,
                              (vk_u64)(unsigned)vk_flags,
                              0,
                              0,
                              0,
                              0));
}

int _close(int fd)
{
    return _vk_ret(vk_syscall(VK_SYS_CLOSE, (vk_u64)(unsigned)fd, 0, 0, 0, 0, 0));
}

int _read(int fd, char* buf, int len)
{
    if (len <= 0) return 0;
    return (int)_vk_ret_ssize(vk_syscall(VK_SYS_READ,
                                         (vk_u64)(unsigned)fd,
                                         (vk_u64)(vk_usize)buf,
                                         (vk_u64)(unsigned)len,
                                         0,
                                         0,
                                         0));
}

int _write(int fd, const char* buf, int len)
{
    if (len <= 0) return 0;
    return (int)_vk_ret_ssize(vk_syscall(VK_SYS_WRITE,
                                         (vk_u64)(unsigned)fd,
                                         (vk_u64)(vk_usize)buf,
                                         (vk_u64)(unsigned)len,
                                         0,
                                         0,
                                         0));
}

int _lseek(int fd, int offset, int whence)
{
    const int vk_whence = _translate_seek_whence(whence);
    if (vk_whence < 0) {
        errno = EINVAL;
        return -1;
    }

    return _vk_ret(vk_syscall(VK_SYS_LSEEK,
                              (vk_u64)(unsigned)fd,
                              (vk_u64)(vk_i64)offset,
                              (vk_u64)(unsigned)vk_whence,
                              0,
                              0,
                              0));
}

int _fstat(int fd, struct stat* st)
{
    vk_stat_t vk_st;
    if (_vk_ret(vk_syscall(VK_SYS_FSTAT,
                           (vk_u64)(unsigned)fd,
                           (vk_u64)(vk_usize)&vk_st,
                           0,
                           0,
                           0,
                           0)) < 0) {
        return -1;
    }

    *st = (struct stat){0};
    st->st_dev = (dev_t)vk_st.st_dev;
    st->st_ino = (ino_t)vk_st.st_ino;
    st->st_size = (off_t)vk_st.st_size;
    st->st_mode = (mode_t)vk_st.st_mode;
    st->st_nlink = (nlink_t)vk_st.st_nlink;
    st->st_blksize = (blksize_t)vk_st.st_blksize;
    return 0;
}

int _stat(const char* path, struct stat* st)
{
    vk_stat_t vk_st;
    if (_vk_ret(vk_syscall(VK_SYS_STAT,
                           (vk_u64)(vk_usize)path,
                           (vk_u64)(vk_usize)&vk_st,
                           0,
                           0,
                           0,
                           0)) < 0) {
        return -1;
    }

    *st = (struct stat){0};
    st->st_dev = (dev_t)vk_st.st_dev;
    st->st_ino = (ino_t)vk_st.st_ino;
    st->st_size = (off_t)vk_st.st_size;
    st->st_mode = (mode_t)vk_st.st_mode;
    st->st_nlink = (nlink_t)vk_st.st_nlink;
    st->st_blksize = (blksize_t)vk_st.st_blksize;
    return 0;
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
    return _vk_ret(vk_syscall(VK_SYS_UNLINK,
                              (vk_u64)(vk_usize)path,
                              0,
                              0,
                              0,
                              0,
                              0));
}

int _isatty(int fd)
{
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

/* ============================================================
 * Time (stub — no RTC yet)
 * ============================================================ */

#include <sys/times.h>

int _gettimeofday(struct timeval* tv, void* tz)
{
    vk_timeval_t vk_tv = {0};
    const vk_i64 ret = vk_syscall(VK_SYS_GETTIMEOFDAY,
                                  (vk_u64)(vk_usize)(tv ? &vk_tv : 0),
                                  (vk_u64)(vk_usize)tz,
                                  0,
                                  0,
                                  0,
                                  0);
    if (_vk_ret(ret) < 0) {
        return -1;
    }
    if (tv) {
        tv->tv_sec = (time_t)vk_tv.tv_sec;
        tv->tv_usec = (suseconds_t)vk_tv.tv_usec;
    }
    return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
    vk_timespec_t vk_ts = {0};
    if (_vk_ret(vk_syscall(VK_SYS_CLOCK_GETTIME,
                           (vk_u64)(unsigned long)clock_id,
                           (vk_u64)(vk_usize)(tp ? &vk_ts : 0),
                           0,
                           0,
                           0,
                           0)) < 0) {
        return -1;
    }

    if (tp) {
        tp->tv_sec = (time_t)vk_ts.tv_sec;
        tp->tv_nsec = (long)vk_ts.tv_nsec;
    }
    return 0;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    const vk_i64 ret = vk_syscall(VK_SYS_MMAP,
                                  (vk_u64)(vk_usize)addr,
                                  (vk_u64)length,
                                  (vk_u64)(unsigned)_translate_prot_flags(prot),
                                  (vk_u64)(unsigned)_translate_mmap_flags(flags),
                                  (vk_u64)(vk_i64)fd,
                                  (vk_u64)(vk_i64)offset);
    if (ret < 0) {
        errno = _vk_errno_to_host((int)(-ret));
        return MAP_FAILED;
    }
    return (void*)(vk_usize)ret;
}

int munmap(void* addr, size_t length)
{
    return _vk_ret(vk_syscall(VK_SYS_MUNMAP,
                              (vk_u64)(vk_usize)addr,
                              (vk_u64)length,
                              0,
                              0,
                              0,
                              0));
}

int mprotect(void* addr, size_t length, int prot)
{
    return _vk_ret(vk_syscall(VK_SYS_MPROTECT,
                              (vk_u64)(vk_usize)addr,
                              (vk_u64)length,
                              (vk_u64)(unsigned)_translate_prot_flags(prot),
                              0,
                              0,
                              0));
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
    return _vk_ret(vk_syscall(VK_SYS_ACCESS,
                              (vk_u64)(vk_usize)path,
                              (vk_u64)(unsigned)mode,
                              0,
                              0,
                              0,
                              0));
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
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }

    return _vk_ret(vk_syscall(VK_SYS_FTRUNCATE,
                              (vk_u64)(unsigned)fd,
                              (vk_u64)(vk_i64)length,
                              0,
                              0,
                              0,
                              0));
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
    if (_vk_ret(vk_syscall(VK_SYS_GETCWD,
                           (vk_u64)(vk_usize)buf,
                           (vk_u64)size,
                           0,
                           0,
                           0,
                           0)) < 0) {
        return 0;
    }
    return buf;
}

ssize_t pread(int fd, void* buf, size_t nbytes, off_t offset)
{
    return _vk_ret_ssize(vk_syscall(VK_SYS_PREAD,
                                    (vk_u64)(unsigned)fd,
                                    (vk_u64)(vk_usize)buf,
                                    (vk_u64)nbytes,
                                    (vk_u64)(vk_i64)offset,
                                    0,
                                    0));
}

ssize_t pwrite(int fd, const void* buf, size_t nbytes, off_t offset)
{
    return _vk_ret_ssize(vk_syscall(VK_SYS_PWRITE,
                                    (vk_u64)(unsigned)fd,
                                    (vk_u64)(vk_usize)buf,
                                    (vk_u64)nbytes,
                                    (vk_u64)(vk_i64)offset,
                                    0,
                                    0));
}

int fcntl(int fd, int cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    void* arg = 0;
    if (cmd == F_GETLK || cmd == F_SETLK || cmd == F_SETLKW) {
        arg = va_arg(args, void*);
    } else if (cmd == F_SETFD || cmd == F_SETFL) {
        arg = (void*)(vk_usize)va_arg(args, int);
    }
    va_end(args);

    const int vk_cmd = _translate_fcntl_cmd(cmd);
    if (vk_cmd < 0) {
        errno = EINVAL;
        return -1;
    }

    const vk_i64 ret = vk_syscall(VK_SYS_FCNTL,
                                  (vk_u64)(unsigned)fd,
                                  (vk_u64)(unsigned)vk_cmd,
                                  (vk_u64)(vk_usize)arg,
                                  0,
                                  0,
                                  0);
    if (ret < 0) {
        errno = _vk_errno_to_host((int)(-ret));
        return -1;
    }

    if (cmd == F_GETFL) {
        return _translate_open_flags_back((int)ret);
    }
    return (int)ret;
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
