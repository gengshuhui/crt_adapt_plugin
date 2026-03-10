/*
 * crtadapt.ld — Linker version script
 *
 * Caps the maximum GLIBC version required by this binary so that
 * dynamic linker on Ubuntu 20 (glibc 2.31) can satisfy all dependencies.
 *
 * Usage:
 *   gcc -o mybin ... -Wl,--version-script=src/crtadapt.ld
 *
 * This does NOT provide missing symbol implementations — it only prevents
 * the linker from emitting version requirements above GLIBC_2.17 for
 * symbols that have both old and new versions available.
 * Combine with crtadapt.c (.symver directives) for full coverage.
 */

GLIBC_2.17 {
    /* intentionally empty — marks 2.17 as the maximum required version */
};
