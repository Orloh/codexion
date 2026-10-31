#!/usr/bin/env python3
import re
import subprocess
import sys
import time

LINE_RE = re.compile(
    r"^(\d+) (\d+) (has taken a dongle|is compiling|is debugging|"
    r"is refactoring|burned out)$"
)
PHASE_SLACK = 50
COOLDOWN_SLACK = 20
BURNOUT_SLACK = 30
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print("FAIL: " + message)


def run(binary, args, timeout=15):
    try:
        start = time.monotonic()
        proc = subprocess.run(
            [binary] + args,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return proc.returncode, proc.stdout, proc.stderr, time.monotonic() - start
    except subprocess.TimeoutExpired:
        return None, "", "timeout", timeout


def parse(out):
    events = []
    for line in out.splitlines():
        match = LINE_RE.match(line)
        check(match is not None, "bad log line: " + repr(line))
        if match:
            events.append(
                (int(match.group(1)), int(match.group(2)), match.group(3))
            )
    return events


def by_coder(events):
    result = {}
    for timestamp, coder_id, message in events:
        result.setdefault(coder_id, []).append((timestamp, message))
    return result


def check_cycles(events, number_of_coders, required):
    coders = by_coder(events)
    check(len(coders) == number_of_coders, "wrong number of coders in logs")
    takes_per_cycle = 1 if number_of_coders == 1 else 2
    for coder_id, coder_events in coders.items():
        messages = [message for _, message in coder_events]
        expected = []
        for _ in range(required):
            expected += ["has taken a dongle"] * takes_per_cycle
            expected += ["is compiling", "is debugging", "is refactoring"]
        check(
            messages == expected,
            "coder %d has wrong state sequence" % coder_id,
        )


def check_no_overlap(events, number_of_coders):
    intervals = []
    for coder_id, coder_events in by_coder(events).items():
        compile_start = None
        for timestamp, message in coder_events:
            if message == "is compiling":
                compile_start = timestamp
            elif message == "is debugging" and compile_start is not None:
                index = coder_id - 1
                dongles = {
                    (index + number_of_coders - 1) % number_of_coders,
                    index % number_of_coders,
                }
                intervals.append((compile_start, timestamp, coder_id, dongles))
                compile_start = None
    for i, first in enumerate(intervals):
        for second in intervals[i + 1:]:
            overlap = first[0] < second[1] and second[0] < first[1]
            if overlap:
                check(
                    not (first[3] & second[3]),
                    "coders %d and %d used the same dongle at once"
                    % (first[2], second[2]),
                )


def check_timing(events, compile_ms, debug_ms, refactor_ms):
    for coder_id, coder_events in by_coder(events).items():
        compile_ts = None
        debug_ts = None
        refactor_ts = None
        for timestamp, message in coder_events:
            if message == "is compiling":
                compile_ts = timestamp
            elif message == "is debugging":
                check(
                    compile_ts is not None
                    and timestamp - compile_ts >= compile_ms - PHASE_SLACK,
                    "coder %d compile duration too short" % coder_id,
                )
                debug_ts = timestamp
            elif message == "is refactoring":
                check(
                    debug_ts is not None
                    and timestamp - debug_ts >= debug_ms - PHASE_SLACK,
                    "coder %d debug duration too short" % coder_id,
                )
                refactor_ts = timestamp
            elif message == "has taken a dongle" and refactor_ts is not None:
                check(
                    timestamp - refactor_ts >= refactor_ms - PHASE_SLACK,
                    "coder %d refactor duration too short" % coder_id,
                )
                refactor_ts = None


def check_cooldown(events, cooldown):
    for coder_id, coder_events in by_coder(events).items():
        for index, (timestamp, message) in enumerate(coder_events):
            if message != "is debugging":
                continue
            for next_ts, next_message in coder_events[index + 1:]:
                if next_message == "has taken a dongle":
                    check(
                        next_ts - timestamp >= cooldown - COOLDOWN_SLACK,
                        "coder %d ignored dongle cooldown" % coder_id,
                    )
                    break


def check_burnout(events, time_to_burnout):
    burned = [event for event in events if event[2] == "burned out"]
    check(len(burned) == 1, "expected exactly one burnout line")
    if len(burned) != 1:
        return
    timestamp, coder_id, _ = burned[0]
    last_compile = 0
    for event_ts, event_coder, message in events:
        if event_coder == coder_id and message == "is compiling":
            last_compile = event_ts
    deadline = last_compile + time_to_burnout
    check(timestamp >= deadline, "burnout logged before deadline")
    check(
        timestamp <= deadline + 10 + BURNOUT_SLACK,
        "burnout logged too late",
    )


def clean_scenario(binary, args, number_of_coders, required,
                   compile_ms, debug_ms, refactor_ms, cooldown):
    rc, out, err, _ = run(binary, args)
    check(rc == 0, "clean scenario returned %r: %s" % (rc, err))
    events = parse(out)
    check(
        all(message != "burned out" for _, _, message in events),
        "clean scenario burned out",
    )
    check_cycles(events, number_of_coders, required)
    check_no_overlap(events, number_of_coders)
    check_timing(events, compile_ms, debug_ms, refactor_ms)
    if cooldown > 0:
        check_cooldown(events, cooldown)


def test_invalid(binary):
    rc, out, err, _ = run(binary, [])
    check(rc == 1, "invalid input did not return 1")
    check(out == "", "invalid input wrote to stdout")
    check(err != "", "invalid input did not write to stderr")


def test_burnout(binary):
    rc, out, _, _ = run(binary, ["1", "50", "100", "100", "100", "1", "0", "edf"])
    check(rc == 0, "burnout run returned nonzero")
    check_burnout(parse(out), 50)


def test_required_zero(binary):
    rc, out, _, _ = run(binary, ["2", "100", "10", "10", "10", "0", "0", "edf"])
    check(rc == 0, "required=0 run returned nonzero")
    check(out == "", "required=0 should not log")


def test_zero_durations(binary):
    rc, out, _, _ = run(binary, ["1", "100", "0", "0", "0", "1", "0", "edf"])
    check(rc == 0, "zero-duration run returned nonzero")
    events = parse(out)
    check_cycles(events, 1, 1)
    check_timing(events, 0, 0, 0)


def test_scheduler(binary):
    clean_scenario(
        binary, ["2", "1000", "50", "50", "50", "2", "0", "fifo"],
        2, 2, 50, 50, 50, 0,
    )
    clean_scenario(
        binary, ["2", "1000", "50", "50", "50", "2", "0", "edf"],
        2, 2, 50, 50, 50, 0,
    )


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "./codexion"
    test_invalid(binary)
    test_required_zero(binary)
    test_zero_durations(binary)
    test_burnout(binary)
    test_scheduler(binary)
    clean_scenario(
        binary, ["1", "500", "10", "10", "10", "2", "100", "edf"],
        1, 2, 10, 10, 10, 100,
    )
    clean_scenario(
        binary, ["4", "1500", "100", "100", "100", "2", "0", "edf"],
        4, 2, 100, 100, 100, 0,
    )
    clean_scenario(
        binary, ["5", "2000", "200", "200", "200", "3", "50", "edf"],
        5, 3, 200, 200, 200, 50,
    )
    if failures:
        print("%d integration checks failed" % len(failures))
        return 1
    print("integration checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
