/*
 * crtadapt.c — GLIBC CRT compatibility adapter
 *
 * Provides local wrapper definitions for symbols whose versioned ABI
 * changed between glibc 2.31 (Ubuntu 20) and glibc 2.35 (Ubuntu 22)
 * in ways that cannot be handled with a simple .symver redirect.
 *
 * Two categories:
 *   1. stat/lstat/fstat  — glibc 2.33 removed stat@GLIBC_2.2.5 and
 *      replaced it with stat@@GLIBC_2.33.  We provide local wrappers
 *      that call __xstat@GLIBC_2.2.5 (still present in both glibc 2.35
 *      and 2.31).
 *
 *   2. __libc_start_main — Ubuntu 22's crt1.o references @@GLIBC_2.34.
 *      We provide a local __libc_start_main that calls @GLIBC_2.2.5
 *      (present in both glibc 2.35 and 2.31).
 *
 * Compile together with your source:
 *   gcc -o mybin src/crtadapt.c main.o ... \
 *       -include src/crtadapt-symver.h \
 *       -pthread -static-libgcc -Wall -O2
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* -----------------------------------------------------------------------
 * stat family
 *
 * __xstat(ver, path, buf) is the versioned glibc internal for stat().
 * ver=1 on x86_64 Linux (STAT_VER_LINUX).
 * __xstat@GLIBC_2.2.5 exists in both glibc 2.35 and glibc 2.31.
 * ----------------------------------------------------------------------- */

#define CRTADAPT_STAT_VER 1   /* _STAT_VER_LINUX for x86_64 */

extern int __xstat_old(int ver, const char *path, struct stat *buf);
__asm__(".symver __xstat_old, __xstat@GLIBC_2.2.5");

extern int __lxstat_old(int ver, const char *path, struct stat *buf);
__asm__(".symver __lxstat_old, __lxstat@GLIBC_2.2.5");

extern int __fxstat_old(int ver, int fd, struct stat *buf);
__asm__(".symver __fxstat_old, __fxstat@GLIBC_2.2.5");

extern int __fxstatat_old(int ver, int dirfd, const char *path,
                          struct stat *buf, int flags);
__asm__(".symver __fxstatat_old, __fxstatat@GLIBC_2.4");

/*
 * Define stat/lstat/fstat/fstatat locally.  The linker will use these
 * local definitions (from crtadapt.o) in preference to the DSO versions,
 * so the binary will only require __xstat@GLIBC_2.2.5 at runtime.
 */
int stat(const char *path, struct stat *buf)
{
    return __xstat_old(CRTADAPT_STAT_VER, path, buf);
}

int lstat(const char *path, struct stat *buf)
{
    return __lxstat_old(CRTADAPT_STAT_VER, path, buf);
}

int fstat(int fd, struct stat *buf)
{
    return __fxstat_old(CRTADAPT_STAT_VER, fd, buf);
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags)
{
    return __fxstatat_old(CRTADAPT_STAT_VER, dirfd, path, buf, flags);
}

/* -----------------------------------------------------------------------
 * __libc_start_main
 *
 * Ubuntu 22's crt1.o references __libc_start_main@@GLIBC_2.34.
 * We provide a local definition that redirects to @GLIBC_2.2.5,
 * which exists in both glibc 2.35 and glibc 2.31.
 *
 * Both the 2.2.5 and 2.34 versions share the same calling convention
 * on x86_64 for the arguments we pass through.
 * ----------------------------------------------------------------------- */

typedef int (*main_fn_t)(int, char **, char **);
typedef void (*void_fn_t)(void);

extern int __libc_start_main_old(main_fn_t main, int argc, char **argv,
                                  void_fn_t init, void_fn_t fini,
                                  void_fn_t rtld_fini, void *stack_end)
    __attribute__((noreturn));
__asm__(".symver __libc_start_main_old, __libc_start_main@GLIBC_2.2.5");

/*
 * Our local __libc_start_main: crt1.o's GLIBC_2.34 reference will resolve
 * to this (direct call at link time), and we forward to GLIBC_2.2.5.
 *
 * MUST be visibility("hidden") — without it the dynamic linker resolves
 * the PLT entry for __libc_start_main@GLIBC_2.2.5 back to this wrapper
 * (it finds the binary's own export first), causing infinite recursion.
 * With hidden visibility, _start still calls us directly (same binary),
 * but the PLT lookup for __libc_start_main_old correctly reaches libc.so.6.
 */
__attribute__((visibility("hidden"), noreturn))
int __libc_start_main(main_fn_t main, int argc, char **argv,
                      void_fn_t init, void_fn_t fini,
                      void_fn_t rtld_fini, void *stack_end)
{
    __libc_start_main_old(main, argc, argv, init, fini, rtld_fini, stack_end);
}
