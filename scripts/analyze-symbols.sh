#!/bin/bash
# analyze-symbols.sh — Show GLIBC version requirements of a compiled binary
#
# Usage: analyze-symbols.sh [binary]
# Default binary: /work/bin/test_compat

set -e

BIN=${1:-/work/bin/test_compat}

if [ ! -f "$BIN" ]; then
    echo "ERROR: binary not found: $BIN" >&2
    exit 1
fi

echo "======================================"
echo " Symbol analysis: $BIN"
echo "======================================"

echo ""
echo "--- File type ---"
file "$BIN"

echo ""
echo "--- All required GLIBC versions ---"
objdump -T "$BIN" \
  | grep -oP 'GLIBC_[0-9]+\.[0-9.]+' \
  | sort -Vu

echo ""
echo "--- Symbols requiring GLIBC >= 2.32 (incompatible with Ubuntu 20) ---"
COMPAT_ISSUES=$(objdump -T "$BIN" \
  | grep -E 'GLIBC_2\.(3[2-9]|[4-9][0-9])' || true)

if [ -z "$COMPAT_ISSUES" ]; then
    echo "(none — binary should be compatible with Ubuntu 20 glibc 2.31)"
else
    echo "$COMPAT_ISSUES"
    echo ""
    HIGHEST=$(echo "$COMPAT_ISSUES" \
      | grep -oP 'GLIBC_[0-9]+\.[0-9.]+' \
      | sort -Vu | tail -1)
    echo "Highest incompatible version required: $HIGHEST"
fi

echo ""
echo "--- Dynamic library dependencies ---"
objdump -p "$BIN" | grep NEEDED || true

echo ""
echo "======================================"
