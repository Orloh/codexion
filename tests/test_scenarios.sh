#!/usr/bin/env bash

set -u

BIN="${1:-./codexion}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-30}"
failures=0

if [[ ! -x "$BIN" ]]; then
	printf 'Binary not found or not executable: %s\n' "$BIN" >&2
	printf 'Run make first, or pass the binary path as the first argument.\n' >&2
	exit 1
fi

printf 'Codexion scenario tests\n'
printf 'Binary: %s\n' "$BIN"
printf '%-30s %-5s %-9s %-9s %-7s %-6s %s\n' \
	"Scenario" "Exit" "Compiles" "Burnouts" "Expect" "Result" "Parameters"
printf '%s\n' \
	"----------------------------------------------------------------------------------------------------------"

while IFS='|' read -r name args expected_burnout expected_compiles; do
	[[ -z "${name// /}" ]] && continue
	output=$(timeout "$TIMEOUT_SECONDS" "$BIN" $args 2>&1)
	rc=$?
	burnouts=$(printf '%s\n' "$output" | grep -c ' burned out$' || true)
	compiles=$(printf '%s\n' "$output" | grep -c ' is compiling$' || true)
	result="PASS"

	if [[ "$rc" -ne 0 ]]; then
		result="FAIL (exit/timeout)"
	elif [[ "$expected_burnout" -eq 1 && "$burnouts" -ne 1 ]]; then
		result="FAIL (expected burnout)"
	elif [[ "$expected_burnout" -eq 0 && "$burnouts" -ne 0 ]]; then
		result="FAIL (unexpected burnout)"
	elif [[ "$expected_compiles" != "-" && "$compiles" -ne "$expected_compiles" ]]; then
		result="FAIL (compile count)"
	fi

	[[ "$result" == "PASS" ]] || failures=$((failures + 1))
	printf '%-30s %-5s %-9s %-9s %-7s %-6s %s\n' \
		"$name" "$rc" "$compiles" "$burnouts" "$expected_burnout" "$result" "$args"

	if [[ "$result" != "PASS" ]]; then
		printf '  args: %s\n' "$args"
		printf '  first output lines:\n'
		printf '%s\n' "$output" | sed -n '1,6p' | sed 's/^/    /'
	fi
done <<'CASES'
1 coder, 3 compiles|1 1000 60 60 60 3 0 edf|0|3
1 coder, burnout|1 60 120 120 120 1 0 edf|1|-
4 coders, 2 compiles each|4 2000 100 100 100 2 0 edf|0|8
4 coders, burnout|4 200 100 100 100 2 0 edf|1|-
50 coders, 1 compile each|50 1000 100 100 100 1 10 edf|0|50
50 coders, burnout|50 50 60 60 60 1 0 edf|1|-
10 coders, 3 compiles each|10 3000 100 100 100 3 20 edf|0|30
10 coders, burnout|10 200 100 100 100 3 0 edf|1|-
CASES

printf '\n'
if [[ "$failures" -eq 0 ]]; then
	printf 'All scenarios passed.\n'
	exit 0
fi

printf '%d scenario(s) failed.\n' "$failures"
exit 1
