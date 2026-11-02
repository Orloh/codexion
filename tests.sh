#!/usr/bin/env bash
set -u

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$ROOT_DIR" || exit 1

BIN=${1:-./codexion}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/codexion-tests.XXXXXX") || exit 1
trap 'rm -rf "$TMP_DIR"' EXIT

PASS=0
FAIL=0
PARAMS=""

pass() {
	PASS=$((PASS + 1))
	if [ -n "$PARAMS" ]; then
		printf 'PASS  %s [%s]\n' "$1" "$PARAMS"
	else
		printf 'PASS  %s\n' "$1"
	fi
}

fail() {
	FAIL=$((FAIL + 1))
	if [ -n "$PARAMS" ]; then
		printf 'FAIL  %s [%s]\n' "$1" "$PARAMS"
	else
		printf 'FAIL  %s\n' "$1"
	fi
}

expect_count() {
	local file="$1"
	local pattern="$2"
	local expected="$3"
	local label="$4"
	local count

	count=$(grep -Ec "$pattern" "$file" || true)
	if [ "$count" -eq "$expected" ]; then
		pass "$label"
	else
		fail "$label (expected $expected, got $count)"
	fi
}

run_case() {
	local out="$1"

	shift
	timeout 30s "$BIN" "$@" >"$out" 2>&1
}

check_valgrind() {
	local label="$1"
	local safe
	local out
	local log
	local status

	shift
	PARAMS="$*"
	safe=$(printf '%s' "$label" | tr ' ' '_')
	out="$TMP_DIR/vg_${safe}.out"
	log="$TMP_DIR/vg_${safe}.log"
	if ! command -v valgrind >/dev/null 2>&1; then
		fail "$label (valgrind not installed)"
		return
	fi
	timeout 120s valgrind --leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=1 \
		"$BIN" "$@" >"$out" 2>"$log"
	status=$?
	if [ "$status" -ne 0 ]; then
		fail "$label (exit $status)"
		grep -E 'definitely lost|indirectly lost|possibly lost|still reachable|ERROR SUMMARY' "$log" | sed 's/^/      /' || true
		return
	fi
	if grep -q 'All heap blocks were freed -- no leaks are possible' "$log" \
		&& grep -q 'ERROR SUMMARY: 0 errors' "$log"; then
		pass "$label"
		grep -E 'All heap blocks were freed -- no leaks are possible|ERROR SUMMARY: 0 errors' "$log" | sed 's/^/      /'
	else
		fail "$label (leaks or valgrind errors)"
		grep -E 'definitely lost|indirectly lost|possibly lost|still reachable|ERROR SUMMARY' "$log" | sed 's/^/      /' || true
	fi
}

make -s || {
	printf 'Build failed\n'
	exit 1
}
if [ ! -x "$BIN" ]; then
	printf 'Binary not found: %s\n' "$BIN"
	exit 1
fi

printf 'Codexion bash tests\n'
printf 'Binary: %s\n' "$BIN"

printf '\n[1] 1 coder, 3 compiles, EDF\n'
PARAMS='1 1000 20 20 20 3 0 edf'
OUT="$TMP_DIR/one_clean.log"
if run_case "$OUT" 1 1000 20 20 20 3 0 edf; then
	pass 'clean run exits 0'
else
	fail 'clean run exits 0'
fi
expect_count "$OUT" ' is compiling$' 3 '1 coder compiles 3 times'
expect_count "$OUT" ' is debugging$' 3 '1 coder debugs 3 times'
expect_count "$OUT" ' is refactoring$' 3 '1 coder refactors 3 times'
expect_count "$OUT" ' has taken a dongle$' 3 '1 coder takes a dongle 3 times'
expect_count "$OUT" ' burned out$' 0 '1 coder does not burn out'

printf '\n[2] 4 coders, 2 compiles each, FIFO with cooldown\n'
PARAMS='4 3000 20 20 20 2 10 fifo'
OUT="$TMP_DIR/four_fifo.log"
if run_case "$OUT" 4 3000 20 20 20 2 10 fifo; then
	pass 'FIFO run exits 0'
else
	fail 'FIFO run exits 0'
fi
expect_count "$OUT" ' is compiling$' 8 '4 coders compile 8 times'
expect_count "$OUT" ' burned out$' 0 '4 coders do not burn out'

printf '\n[3] 50 coders, 1 compile each, EDF\n'
PARAMS='50 1000 50 50 50 1 10 edf'
OUT="$TMP_DIR/fifty_edf.log"
if run_case "$OUT" 50 1000 50 50 50 1 10 edf; then
	pass '50-coder run exits 0'
else
	fail '50-coder run exits 0'
fi
expect_count "$OUT" ' is compiling$' 50 '50 coders compile once'
expect_count "$OUT" ' burned out$' 0 '50 coders do not burn out'

printf '\n[4] burnout example\n'
PARAMS='1 60 120 120 120 1 0 edf'
OUT="$TMP_DIR/burnout.log"
if run_case "$OUT" 1 60 120 120 120 1 0 edf; then
	pass 'burnout run exits 0'
else
	fail 'burnout run exits 0'
fi
expect_count "$OUT" ' burned out$' 1 'burnout is reported once'

printf '\n[5] invalid scheduler\n'
PARAMS='1 1000 20 20 20 1 0 invalid'
OUT="$TMP_DIR/invalid.log"
run_case "$OUT" 1 1000 20 20 20 1 0 invalid
STATUS=$?
if [ "$STATUS" -eq 1 ]; then
	pass 'invalid scheduler exits 1'
else
	fail "invalid scheduler exits 1 (got $STATUS)"
fi
expect_count "$OUT" '^Error: scheduler must be exactly fifo or edf$' 1 'invalid scheduler reports an error'

printf '\n[6] zero required compiles\n'
PARAMS='3 1000 20 20 20 0 0 fifo'
OUT="$TMP_DIR/zero.log"
if run_case "$OUT" 3 1000 20 20 20 0 0 fifo; then
	pass 'zero-compile run exits 0'
else
	fail 'zero-compile run exits 0'
fi
expect_count "$OUT" ' burned out$' 0 'zero-compile run does not burn out'

printf '\n[7] valgrind memory checks\n'
check_valgrind 'clean run has no leaks' 4 5000 20 20 20 2 0 fifo
check_valgrind 'burnout run has no leaks' 1 60 120 120 120 1 0 edf

printf '\nSummary: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
