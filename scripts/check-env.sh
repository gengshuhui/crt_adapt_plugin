#!/bin/bash
# check-env.sh — Print GCC and GLIBC version info from the current environment

set -e

LIBC=$(find /lib /lib64 /usr/lib -name 'libc.so.6' 2>/dev/null | head -1)
LIBC=${LIBC:-/lib/x86_64-linux-gnu/libc.so.6}

echo "======================================"
echo " Environment: $(uname -n)"
echo "======================================"

echo ""
echo "--- GCC version ---"
gcc --version 2>/dev/null || echo "(gcc not installed)"

echo ""
echo "--- ldd (GLIBC runtime) version ---"
ldd --version 2>/dev/null | head -1

echo ""
echo "--- libc6 package ---"
dpkg -l libc6 2>/dev/null | grep '^ii' || echo "(dpkg not available)"

echo ""
echo "--- GLIBC version nodes in $LIBC ---"
objdump -T "$LIBC" 2>/dev/null \
  | grep -oP 'GLIBC_[0-9]+\.[0-9.]+' \
  | sort -Vu \
  || echo "(objdump not available)"

echo ""
echo "======================================"
