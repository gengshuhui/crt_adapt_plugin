/*
 * crtadapt-symver.h — GLIBC symbol version pins
 *
 * Inject into every compiled TU via:  gcc -include src/crtadapt-symver.h
 *
 * Only symbols whose old @VERSION entry still exists in glibc 2.35
 * can be redirected this way.  Symbols that were restructured (stat
 * family via __xstat, __libc_start_main) are handled in crtadapt.c
 * as local wrapper definitions instead.
 */

#ifndef CRTADAPT_SYMVER_H
#define CRTADAPT_SYMVER_H

/* pthread: default @@GLIBC_2.34 → pin to @GLIBC_2.2.5 / @GLIBC_2.3.2
 * (Both old and new versions coexist in glibc 2.35) */
__asm__(".symver pthread_create,         pthread_create@GLIBC_2.2.5");
__asm__(".symver pthread_join,           pthread_join@GLIBC_2.2.5");
__asm__(".symver pthread_detach,         pthread_detach@GLIBC_2.2.5");
__asm__(".symver pthread_exit,           pthread_exit@GLIBC_2.2.5");
__asm__(".symver pthread_self,           pthread_self@GLIBC_2.2.5");
__asm__(".symver pthread_mutex_init,     pthread_mutex_init@GLIBC_2.2.5");
__asm__(".symver pthread_mutex_destroy,  pthread_mutex_destroy@GLIBC_2.2.5");
__asm__(".symver pthread_mutex_lock,     pthread_mutex_lock@GLIBC_2.2.5");
__asm__(".symver pthread_mutex_unlock,   pthread_mutex_unlock@GLIBC_2.2.5");
__asm__(".symver pthread_cond_init,      pthread_cond_init@GLIBC_2.3.2");
__asm__(".symver pthread_cond_destroy,   pthread_cond_destroy@GLIBC_2.3.2");
__asm__(".symver pthread_cond_wait,      pthread_cond_wait@GLIBC_2.3.2");
__asm__(".symver pthread_cond_signal,    pthread_cond_signal@GLIBC_2.3.2");
__asm__(".symver pthread_cond_broadcast, pthread_cond_broadcast@GLIBC_2.3.2");
__asm__(".symver pthread_sigmask,        pthread_sigmask@GLIBC_2.2.5");

/*
 * stat family: stat@GLIBC_2.2.5 was REMOVED from glibc 2.33 (it was an
 * alias for __xstat).  glibc 2.33+ only has stat@@GLIBC_2.33.  So we
 * cannot use .symver here.  Instead, crtadapt.c provides local wrapper
 * definitions of stat/lstat/fstat that call __xstat@GLIBC_2.2.5.
 *
 * __libc_start_main: handled in crtadapt.c as a local wrapper.
 */

#endif /* CRTADAPT_SYMVER_H */
