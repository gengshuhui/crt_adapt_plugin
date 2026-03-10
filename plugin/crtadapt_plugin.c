/*
 * crtadapt_plugin.c — GCC plugin for transparent GLIBC backward compatibility
 *
 * Makes any binary compiled on Ubuntu 22 (GCC 11, glibc 2.35) run on
 * Ubuntu 20 (glibc 2.31) without modifying source code or build scripts
 * beyond adding -fplugin=/path/to/crtadapt.so.
 *
 * Mechanism: hook PLUGIN_FINISH_UNIT and write raw assembly directives into
 * the current TU's asm_out_file.  Three categories of fixup:
 *
 *   1. pthread_* — .symver pins to GLIBC_2.2.5 / GLIBC_2.3.2.
 *      These old version slots still exist in glibc 2.35, so a plain
 *      .symver redirect is sufficient.
 *
 *   2. stat/lstat/fstat/fstat64/fstatat — COMDAT wrappers.
 *      stat@GLIBC_2.2.5 was removed in glibc 2.33; we route through
 *      __xstat@GLIBC_2.2.5 which still exists in both glibc 2.35 and 2.31.
 *      COMDAT (.text.stat,"axG",@progbits,stat,comdat) ensures the linker
 *      keeps exactly one copy when multiple TUs are compiled with the plugin.
 *
 *   3. __libc_start_main — hidden COMDAT wrapper.
 *      Ubuntu 22's crt1.o references __libc_start_main@@GLIBC_2.34.
 *      Our local hidden wrapper intercepts that call and tail-calls
 *      __libc_start_main@GLIBC_2.2.5 in libc.so.6.
 *      MUST be .hidden: default visibility causes the dynamic linker's PLT
 *      lookup for __libc_start_main@GLIBC_2.2.5 to resolve back to this
 *      wrapper → infinite recursion → SIGSEGV on startup.
 *
 * Build (inside Ubuntu 22 container with gcc-11-plugin-dev):
 *   gcc -I$(gcc -print-file-name=plugin)/include \
 *       -fPIC -O2 -Wall -shared -o crtadapt.so crtadapt_plugin.c
 *
 * Use:
 *   gcc -fplugin=/path/to/crtadapt.so \
 *       -pthread -static-libgcc -O2 \
 *       -o mybinary mysource.c
 *   patchelf --add-needed libpthread.so.0 mybinary
 */

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "output.h"         /* asm_out_file */

int plugin_is_GPL_compatible;

/* -------------------------------------------------------------------------
 * crtadapt_finish_unit — emit all compatibility fixups for this TU
 * ------------------------------------------------------------------------- */
static void
crtadapt_finish_unit(void *gcc_data __attribute__((unused)),
                     void *user_data __attribute__((unused)))
{
    FILE *f = asm_out_file;

    /* ── 1. pthread symbol version pins ────────────────────────────────────
     *
     * Redirect all pthread_* references in this TU to the GLIBC_2.2.5 /
     * GLIBC_2.3.2 version slots, which exist in both glibc 2.31 and 2.35.
     * Without this, Ubuntu 22's linker uses the @@GLIBC_2.34 default
     * (libpthread merged into libc.so.6), which glibc 2.31 does not have.
     */
    fputs("\n\t/* crtadapt: pthread version pins */\n", f);
    fputs("\t.symver pthread_create,         pthread_create@GLIBC_2.2.5\n",   f);
    fputs("\t.symver pthread_join,           pthread_join@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_detach,         pthread_detach@GLIBC_2.2.5\n",   f);
    fputs("\t.symver pthread_exit,           pthread_exit@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_self,           pthread_self@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_mutex_init,     pthread_mutex_init@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_mutex_destroy,  pthread_mutex_destroy@GLIBC_2.2.5\n",  f);
    fputs("\t.symver pthread_mutex_lock,     pthread_mutex_lock@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_mutex_unlock,   pthread_mutex_unlock@GLIBC_2.2.5\n",   f);
    fputs("\t.symver pthread_cond_init,      pthread_cond_init@GLIBC_2.3.2\n",      f);
    fputs("\t.symver pthread_cond_destroy,   pthread_cond_destroy@GLIBC_2.3.2\n",   f);
    fputs("\t.symver pthread_cond_wait,      pthread_cond_wait@GLIBC_2.3.2\n",      f);
    fputs("\t.symver pthread_cond_signal,    pthread_cond_signal@GLIBC_2.3.2\n",    f);
    fputs("\t.symver pthread_cond_broadcast, pthread_cond_broadcast@GLIBC_2.3.2\n", f);
    fputs("\t.symver pthread_sigmask,        pthread_sigmask@GLIBC_2.2.5\n",        f);
    /* C++ extras — re-versioned in GLIBC_2.34 along with the merge */
    fputs("\t.symver pthread_once,           pthread_once@GLIBC_2.2.5\n",           f);
    fputs("\t.symver pthread_key_create,     pthread_key_create@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_key_delete,     pthread_key_delete@GLIBC_2.2.5\n",     f);
    fputs("\t.symver pthread_getspecific,    pthread_getspecific@GLIBC_2.2.5\n",    f);
    fputs("\t.symver pthread_setspecific,    pthread_setspecific@GLIBC_2.2.5\n",    f);
    fputs("\t.symver pthread_attr_init,      pthread_attr_init@GLIBC_2.2.5\n",      f);
    fputs("\t.symver pthread_attr_destroy,   pthread_attr_destroy@GLIBC_2.2.5\n",   f);
    fputs("\t.symver pthread_rwlock_init,    pthread_rwlock_init@GLIBC_2.2.5\n",    f);
    fputs("\t.symver pthread_rwlock_destroy, pthread_rwlock_destroy@GLIBC_2.2.5\n", f);
    fputs("\t.symver pthread_rwlock_rdlock,  pthread_rwlock_rdlock@GLIBC_2.2.5\n",  f);
    fputs("\t.symver pthread_rwlock_wrlock,  pthread_rwlock_wrlock@GLIBC_2.2.5\n",  f);
    fputs("\t.symver pthread_rwlock_unlock,  pthread_rwlock_unlock@GLIBC_2.2.5\n",  f);

    /* ── 2. stat family: versioned references used by COMDAT wrappers ──────
     *
     * These declare the private internal names we use inside the wrappers.
     * .symver binds any reference to __crtadapt_xstat (etc.) in this TU
     * to the correspondingly versioned glibc internal.
     */
    fputs("\n\t/* crtadapt: versioned references for __xstat family */\n", f);
    fputs("\t.symver __crtadapt_xstat,    __xstat@GLIBC_2.2.5\n",    f);
    fputs("\t.symver __crtadapt_lxstat,   __lxstat@GLIBC_2.2.5\n",   f);
    fputs("\t.symver __crtadapt_fxstat,   __fxstat@GLIBC_2.2.5\n",   f);
    fputs("\t.symver __crtadapt_fxstatat, __fxstatat@GLIBC_2.4\n",   f);

    /* stat(path, buf)  →  __xstat(1, path, buf)
     *
     * x86-64 SysV ABI argument mapping:
     *   stat:    rdi=path, rsi=buf
     *   __xstat: rdi=ver,  rsi=path, rdx=buf
     * Transform: rsi→rdx, rdi→rsi, $1→edi                           */
    fputs("\n\t/* crtadapt: stat() COMDAT wrapper */\n", f);
    fputs("\t.section .text.stat,\"axG\",@progbits,stat,comdat\n",    f);
    fputs("\t.globl stat\n",                                           f);
    fputs("\t.type  stat, @function\n",                                f);
    fputs("stat:\n",                                                   f);
    fputs("\t.cfi_startproc\n",                                        f);
    fputs("\tmovq %rsi, %rdx\n",                                       f);
    fputs("\tmovq %rdi, %rsi\n",                                       f);
    fputs("\tmovl $1, %edi\n",                                         f);
    fputs("\tjmp  __crtadapt_xstat\n",                                 f);
    fputs("\t.cfi_endproc\n",                                          f);
    fputs("\t.size stat, .-stat\n",                                    f);

    /* lstat(path, buf)  →  __lxstat(1, path, buf)  — same register map */
    fputs("\n\t/* crtadapt: lstat() COMDAT wrapper */\n", f);
    fputs("\t.section .text.lstat,\"axG\",@progbits,lstat,comdat\n",  f);
    fputs("\t.globl lstat\n",                                          f);
    fputs("\t.type  lstat, @function\n",                               f);
    fputs("lstat:\n",                                                  f);
    fputs("\t.cfi_startproc\n",                                        f);
    fputs("\tmovq %rsi, %rdx\n",                                       f);
    fputs("\tmovq %rdi, %rsi\n",                                       f);
    fputs("\tmovl $1, %edi\n",                                         f);
    fputs("\tjmp  __crtadapt_lxstat\n",                                f);
    fputs("\t.cfi_endproc\n",                                          f);
    fputs("\t.size lstat, .-lstat\n",                                  f);

    /* fstat(fd, buf)  →  __fxstat(1, fd, buf)
     *   fstat:    rdi=fd,  rsi=buf
     *   __fxstat: rdi=ver, rsi=fd,  rdx=buf
     * Transform: rsi→rdx, rdi→rsi, $1→edi                           */
    fputs("\n\t/* crtadapt: fstat() COMDAT wrapper */\n", f);
    fputs("\t.section .text.fstat,\"axG\",@progbits,fstat,comdat\n",  f);
    fputs("\t.globl fstat\n",                                          f);
    fputs("\t.type  fstat, @function\n",                               f);
    fputs("fstat:\n",                                                  f);
    fputs("\t.cfi_startproc\n",                                        f);
    fputs("\tmovq %rsi, %rdx\n",                                       f);
    fputs("\tmovq %rdi, %rsi\n",                                       f);
    fputs("\tmovl $1, %edi\n",                                         f);
    fputs("\tjmp  __crtadapt_fxstat\n",                                f);
    fputs("\t.cfi_endproc\n",                                          f);
    fputs("\t.size fstat, .-fstat\n",                                  f);

    /* fstat64: on Linux x86-64 struct stat == struct stat64; reuse __fxstat */
    fputs("\n\t/* crtadapt: fstat64() COMDAT wrapper (C++ / _FILE_OFFSET_BITS=64) */\n", f);
    fputs("\t.section .text.fstat64,\"axG\",@progbits,fstat64,comdat\n", f);
    fputs("\t.globl fstat64\n",                                           f);
    fputs("\t.type  fstat64, @function\n",                                f);
    fputs("fstat64:\n",                                                   f);
    fputs("\t.cfi_startproc\n",                                           f);
    fputs("\tmovq %rsi, %rdx\n",                                          f);
    fputs("\tmovq %rdi, %rsi\n",                                          f);
    fputs("\tmovl $1, %edi\n",                                            f);
    fputs("\tjmp  __crtadapt_fxstat\n",                                   f);
    fputs("\t.cfi_endproc\n",                                             f);
    fputs("\t.size fstat64, .-fstat64\n",                                 f);

    /* fstatat(dirfd, path, buf, flags)  →  __fxstatat(1, dirfd, path, buf, flags)
     *   fstatat:    rdi=dirfd, rsi=path, rdx=buf,  rcx=flags
     *   __fxstatat: rdi=ver,   rsi=dirfd, rdx=path, rcx=buf, r8=flags
     * Transform: rcx→r8, rdx→rcx, rsi→rdx, rdi→rsi, $1→edi         */
    fputs("\n\t/* crtadapt: fstatat() COMDAT wrapper */\n", f);
    fputs("\t.section .text.fstatat,\"axG\",@progbits,fstatat,comdat\n", f);
    fputs("\t.globl fstatat\n",                                           f);
    fputs("\t.type  fstatat, @function\n",                                f);
    fputs("fstatat:\n",                                                   f);
    fputs("\t.cfi_startproc\n",                                           f);
    fputs("\tmovq %rcx, %r8\n",                                           f);
    fputs("\tmovq %rdx, %rcx\n",                                          f);
    fputs("\tmovq %rsi, %rdx\n",                                          f);
    fputs("\tmovq %rdi, %rsi\n",                                          f);
    fputs("\tmovl $1, %edi\n",                                            f);
    fputs("\tjmp  __crtadapt_fxstatat\n",                                 f);
    fputs("\t.cfi_endproc\n",                                             f);
    fputs("\t.size fstatat, .-fstatat\n",                                 f);

    /* ── 3. __libc_start_main — hidden COMDAT wrapper ──────────────────────
     *
     * Ubuntu 22's crt1.o calls __libc_start_main@@GLIBC_2.34.  The linker
     * resolves this to our local definition at link time (local defs win
     * over versioned DSO references).  We then tail-call
     * __libc_start_main@GLIBC_2.2.5 which exists in both glibc 2.35 and 2.31.
     *
     * The calling convention is identical for both versions on x86-64, so a
     * plain jmp (tail call) suffices — no argument shuffling needed.
     *
     * .hidden is mandatory: if the symbol were globally visible, the dynamic
     * linker would resolve the PLT entry for __libc_start_main@GLIBC_2.2.5
     * back to our wrapper (the binary's own export takes priority), causing
     * infinite recursion.  With .hidden the PLT lookup skips this binary and
     * finds the real entry in libc.so.6.
     */
    fputs("\n\t/* crtadapt: __libc_start_main versioned reference */\n", f);
    fputs("\t.symver __crtadapt_lsm, __libc_start_main@GLIBC_2.2.5\n",  f);

    fputs("\n\t/* crtadapt: __libc_start_main hidden COMDAT wrapper */\n", f);
    fputs("\t.section .text.__libc_start_main,\"axG\",@progbits,__libc_start_main,comdat\n", f);
    fputs("\t.hidden __libc_start_main\n",                                  f);
    fputs("\t.globl  __libc_start_main\n",                                  f);
    fputs("\t.type   __libc_start_main, @function\n",                       f);
    fputs("__libc_start_main:\n",                                           f);
    fputs("\t.cfi_startproc\n",                                             f);
    fputs("\tjmp __crtadapt_lsm\n",                                         f);
    fputs("\t.cfi_endproc\n",                                               f);
    fputs("\t.size __libc_start_main, .-__libc_start_main\n",               f);

    fputs("\n\t.previous\n", f);
}

/* -------------------------------------------------------------------------
 * plugin_init — register callbacks
 * ------------------------------------------------------------------------- */
int
plugin_init(struct plugin_name_args   *plugin_info,
            struct plugin_gcc_version *version)
{
    if (!plugin_default_version_check(version, &gcc_version))
        return 1;

    register_callback(plugin_info->base_name,
                      PLUGIN_FINISH_UNIT,
                      crtadapt_finish_unit,
                      NULL);
    return 0;
}
