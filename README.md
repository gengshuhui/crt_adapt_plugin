# crtadapt — GLIBC CRT Compatibility Adapter

Compile on **Ubuntu 22.04** (GCC 11, glibc 2.35), run on **Ubuntu 20.04** (glibc 2.31) — without cross-compilation, without static linking, without modifying your source code.

---

## The Problem

GCC on Ubuntu 22 links against glibc 2.35 by default. Several symbols received new versioned ABI entries between glibc 2.31 and 2.35 that do not exist in the older runtime:

| Symbol | Default version on Ubuntu 22 | Status on Ubuntu 20 |
|--------|------------------------------|---------------------|
| `stat` / `fstat` / `lstat` | `@@GLIBC_2.33` | Missing — `stat@GLIBC_2.2.5` was **removed** in 2.33 |
| `pthread_create` / `pthread_join` / … | `@@GLIBC_2.34` | Missing — libpthread merged into libc in 2.34 |
| `__libc_start_main` | `@@GLIBC_2.34` | Missing — Ubuntu 22's `crt1.o` emits the new reference |

Running an unadapted Ubuntu 22 binary on Ubuntu 20 produces:

```
/lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.33' not found
/lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.34' not found
```

---

## The Solution

crtadapt provides two equivalent approaches that redirect symbol references to older versioned ABI entries present in **both** glibc 2.35 and 2.31:

| Approach | Files | How to use |
|----------|-------|-----------|
| **Manual adapter** | `src/crtadapt.c`, `src/crtadapt-symver.h` | Add to gcc command line |
| **GCC Plugin** | `plugin/crtadapt_plugin.c` | Single `-fplugin=` flag |

Both produce a binary with maximum required GLIBC version of **2.25** (`getrandom`).

---

## Quick Start

All steps run inside Docker — no Ubuntu 22 or Ubuntu 20 installation required.

```bash
# Full manual adapter pipeline
make all

# Full GCC plugin pipeline
make build-plugin build-plugin-adapted analyze-plugin test-plugin
```

---

## Makefile Targets

| Target | Description |
|--------|-------------|
| `make check` | Print GCC + GLIBC versions in both Ubuntu 22 and Ubuntu 20 containers |
| `make build` | Compile `test_compat` (no adapter) in Ubuntu 22 |
| `make analyze` | Show GLIBC symbol requirements of the unadapted binary |
| `make test-native` | Run unadapted binary on Ubuntu 20 — expected to fail |
| `make build-adapted` | Compile with `crtadapt.c` + `-include crtadapt-symver.h` |
| `make test-adapted` | Run adapted binary on Ubuntu 20 — expected to pass |
| `make build-plugin` | Compile `plugin/crtadapt.so` (requires `gcc-11-plugin-dev`) |
| `make build-plugin-adapted` | Compile `test_compat` with `-fplugin=crtadapt.so` |
| `make analyze-plugin` | Show GLIBC symbol requirements of the plugin-adapted binary |
| `make test-plugin` | Run plugin-adapted binary on Ubuntu 20 — expected to pass |
| `make all` | Full manual adapter pipeline |
| `make clean` | Remove compiled binaries |

---

## Approach 1 — Manual Adapter

Add two items to your existing GCC command:

```bash
gcc -o mybin \
    src/crtadapt.c \        # provides stat/lstat/fstat wrappers + __libc_start_main
    mysource.c \
    -include src/crtadapt-symver.h \   # pins pthread_* to GLIBC_2.2.5
    -pthread -static-libgcc -O2

patchelf --add-needed libpthread.so.0 mybin
```

**`src/crtadapt.c`** provides local wrapper definitions for symbols that cannot be handled by `.symver` alone:

- `stat` / `lstat` / `fstat` / `fstatat` — route through `__xstat@GLIBC_2.2.5` (still present in glibc 2.35)
- `__libc_start_main` — intercepts `crt1.o`'s `@@GLIBC_2.34` reference and forwards to `@GLIBC_2.2.5`; **must be `visibility("hidden")`** to prevent PLT infinite recursion

**`src/crtadapt-symver.h`** (injected via `-include` into every TU) contains `.symver` directives that pin the `pthread_*` family to their pre-2.34 version slots, which still coexist in glibc 2.35.

---

## Approach 2 — GCC Plugin (recommended)

Build the plugin once, then use it transparently with a single flag:

```bash
# Build the plugin (inside Ubuntu 22, requires gcc-11-plugin-dev)
g++ -I$(gcc -print-file-name=plugin)/include \
    -fPIC -O2 -Wall -shared \
    -o plugin/crtadapt.so plugin/crtadapt_plugin.c

# Compile any source — zero changes to your code
gcc -fplugin=/path/to/crtadapt.so \
    -pthread -static-libgcc -O2 \
    -o mybin mysource.c

patchelf --add-needed libpthread.so.0 mybin
```

### How the plugin works

The plugin hooks `PLUGIN_FINISH_UNIT` and writes raw assembly into `asm_out_file` (the current TU's assembler output stream) at the end of each compiled translation unit:

1. **pthread `.symver` pins** — emits `.symver pthread_create, pthread_create@GLIBC_2.2.5` (and ~20 others) into every TU, redirecting all pthread references to the GLIBC_2.2.5 / GLIBC_2.3.2 version slots.

2. **stat family COMDAT wrappers** — emits COMDAT assembly sections for `stat`, `lstat`, `fstat`, `fstat64`, and `fstatat` that tail-call `__xstat@GLIBC_2.2.5`. The COMDAT group flag (`.text.stat,"axG",@progbits,stat,comdat`) ensures the linker keeps exactly one copy when multiple TUs are compiled with the plugin.

3. **`__libc_start_main` hidden COMDAT wrapper** — intercepts the `@@GLIBC_2.34` reference from Ubuntu 22's `crt1.o` and tail-calls `__libc_start_main@GLIBC_2.2.5`. The `.hidden` directive is mandatory to prevent the dynamic linker from resolving the PLT entry back to this wrapper (which would cause infinite recursion on startup).

---

## Why `patchelf --add-needed libpthread.so.0`?

On Ubuntu 22, `libpthread.so.0` is a 21 KB stub containing only version anchor symbols — all actual pthread functions have moved into `libc.so.6`. The GNU linker omits `libpthread.so.0` from `DT_NEEDED` even with `--no-as-needed` because it contributes no function symbols.

On Ubuntu 20, `libpthread.so.0` still contains the actual `pthread_create@GLIBC_2.2.5` etc. implementations. Without `libpthread.so.0` in `DT_NEEDED`, the dynamic linker on Ubuntu 20 will not load it and symbol lookup fails.

`patchelf --add-needed libpthread.so.0` injects the entry into the binary's `DT_NEEDED` list after linking.

---

## Project Layout

```
crtadapt/
├── Makefile                   # Full pipeline orchestration
├── docker/
│   ├── Dockerfile.build       # ubuntu:22.04 + gcc/g++/binutils/patchelf/gcc-11-plugin-dev
│   └── Dockerfile.test        # ubuntu:20.04 minimal (no dev tools)
├── scripts/
│   ├── check-env.sh           # Print GCC + GLIBC versions inside a container
│   └── analyze-symbols.sh     # Report required GLIBC versions for a binary
├── src/
│   ├── crtadapt.c             # Manual adapter: stat wrappers + __libc_start_main
│   ├── crtadapt-symver.h      # Manual adapter: pthread .symver directives
│   └── crtadapt.ld            # Linker version script (reference; not used in adapted build)
├── plugin/
│   ├── crtadapt_plugin.c      # GCC plugin source (C++, compiled with g++)
│   └── Makefile               # Plugin build (standalone)
└── test/
    └── test_compat.c          # Test program: malloc, stat, pthread, getrandom
```

---

## Verified Results

```
Ubuntu 22 build (GCC 11.4.0, glibc 2.35)  →  Ubuntu 20 runtime (glibc 2.31)
```

| Binary | Max GLIBC required | Ubuntu 20 result |
|--------|--------------------|-----------------|
| `test_compat` (no adapter) | GLIBC_2.34 | FAIL — version not found |
| `test_compat_adapted` (manual) | GLIBC_2.25 | **All 4 tests PASS** |
| `test_compat_plugin` (plugin) | GLIBC_2.25 | **All 4 tests PASS** |

Tests: `malloc/free`, `stat("/proc/self/exe")`, `pthread_create/join`, `getrandom`.
