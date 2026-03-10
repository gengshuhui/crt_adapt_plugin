/*
 * test_compat.c — Compatibility test program
 *
 * Exercises APIs that commonly trigger GLIBC version bumps between
 * Ubuntu 20 (glibc 2.31) and Ubuntu 22 (glibc 2.35):
 *
 *   - printf / malloc / errno        (GLIBC_2.2.5 — always fine)
 *   - stat / fstat                   (changed in GLIBC_2.33)
 *   - pthread_create / pthread_join  (re-versioned in GLIBC_2.34)
 *   - getrandom                      (GLIBC_2.25, safe replacement for arc4random)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/random.h>
#include <unistd.h>

/* --- pthread test --- */
static void *thread_fn(void *arg)
{
    int *val = (int *)arg;
    *val = 42;
    return NULL;
}

static int test_pthread(void)
{
    pthread_t tid;
    int result = 0;

    if (pthread_create(&tid, NULL, thread_fn, &result) != 0) {
        fprintf(stderr, "FAIL pthread_create: %s\n", strerror(errno));
        return 1;
    }
    pthread_join(tid, NULL);

    if (result != 42) {
        fprintf(stderr, "FAIL pthread result: expected 42, got %d\n", result);
        return 1;
    }
    printf("PASS pthread_create / pthread_join\n");
    return 0;
}

/* --- stat test --- */
static int test_stat(void)
{
    struct stat st;
    if (stat("/proc/self/exe", &st) != 0) {
        fprintf(stderr, "FAIL stat: %s\n", strerror(errno));
        return 1;
    }
    printf("PASS stat  (size=%ld bytes)\n", (long)st.st_size);
    return 0;
}

/* --- getrandom test (safe replacement for arc4random on Ubuntu 20) --- */
static int test_getrandom(void)
{
    unsigned char buf[8];
    ssize_t n = getrandom(buf, sizeof(buf), 0);
    if (n != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "FAIL getrandom: %s\n", strerror(errno));
        return 1;
    }
    printf("PASS getrandom (first byte=0x%02x)\n", buf[0]);
    return 0;
}

/* --- malloc / free test --- */
static int test_malloc(void)
{
    char *p = malloc(1024);
    if (!p) {
        fprintf(stderr, "FAIL malloc\n");
        return 1;
    }
    memset(p, 0xAB, 1024);
    free(p);
    printf("PASS malloc / free\n");
    return 0;
}

int main(void)
{
    int failures = 0;

    printf("=== crtadapt compatibility test ===\n");
    printf("Running on PID %d\n\n", getpid());

    failures += test_malloc();
    failures += test_stat();
    failures += test_pthread();
    failures += test_getrandom();

    printf("\n");
    if (failures == 0) {
        printf("All tests PASSED.\n");
        return 0;
    } else {
        printf("%d test(s) FAILED.\n", failures);
        return 1;
    }
}
