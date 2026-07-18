#!/bin/bash
set -euo pipefail

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

NEWRER="${1:-./newer}"

# Test 1: basic copy from newer to older
echo "old" > "$TMPDIR/a"
echo "new" > "$TMPDIR/b"
touch -d "2000-01-01" "$TMPDIR/a"
touch -d "2020-01-01" "$TMPDIR/b"
$NEWRER "$TMPDIR/a" "$TMPDIR/b"
[[ "$(cat "$TMPDIR/a")" == "new" ]]
echo "PASS: basic copy"

# Test 2: permission sync
echo "permtest" > "$TMPDIR/b"
chmod 700 "$TMPDIR/b"
echo "x" > "$TMPDIR/c"
touch -d "1999-01-01" "$TMPDIR/c"
touch -d "2021-01-01" "$TMPDIR/b"
$NEWRER "$TMPDIR/c" "$TMPDIR/b"
[[ "$(cat "$TMPDIR/c")" == "permtest" ]]
mode=$(stat -c '%a' "$TMPDIR/c")
[[ "$mode" == "700" ]]
echo "PASS: permission sync"

# Test 3: same file detection
echo "same" > "$TMPDIR/s"
if $NEWRER "$TMPDIR/s" "$TMPDIR/s" 2>/dev/null; then
    echo "FAIL: same file not rejected"
    exit 1
fi
echo "PASS: same file detection"

# Test 4: dry-run
echo "dry-old" > "$TMPDIR/d1"
echo "dry-new" > "$TMPDIR/d2"
touch -d "2022-01-01" "$TMPDIR/d1"
touch -d "2022-01-02" "$TMPDIR/d2"
$NEWRER -n "$TMPDIR/d1" "$TMPDIR/d2"
[[ "$(cat "$TMPDIR/d1")" == "dry-old" ]]
echo "PASS: dry-run"

# Test 5: verbose output
echo "v-old" > "$TMPDIR/v1"
echo "v-new" > "$TMPDIR/v2"
touch -d "2022-01-01" "$TMPDIR/v1"
touch -d "2022-01-02" "$TMPDIR/v2"
output=$($NEWRER -v "$TMPDIR/v1" "$TMPDIR/v2")
[[ "$output" == *"v2"* ]] || { echo "FAIL: verbose output: $output"; exit 1; }
echo "PASS: verbose"

# Test 6: atomic mode
echo "a-old" > "$TMPDIR/a1"
echo "a-new" > "$TMPDIR/a2"
touch -d "2022-01-01" "$TMPDIR/a1"
touch -d "2022-01-03" "$TMPDIR/a2"
$NEWRER -a "$TMPDIR/a1" "$TMPDIR/a2"
[[ "$(cat "$TMPDIR/a1")" == "a-new" ]]
echo "PASS: atomic mode"

# Test 7: missing file copy
echo "missing-src" > "$TMPDIR/m1"
$NEWRER "$TMPDIR/m1" "$TMPDIR/m2"
[[ "$(cat "$TMPDIR/m2")" == "missing-src" ]]
echo "PASS: missing file copy"

# Test 8: help
$NEWRER -h >/dev/null
echo "PASS: help"

echo "ALL TESTS PASSED"
