# AGENTS.md — Codexion Development Playbook

42 "codexion": a C/pthreads simulation where coder threads compete for dongles.
`summary.md` is the authoritative design/spec — read it before implementing.

## Current status & build order

Implemented:
- `include/codexion.h`: shared structs, `t_policy` (FIFO/EDF), `t_shared`,
  `t_dongle.mutex`, `dongles_ready`, heap/parse/log/time API.
- `src/heap.c`: min-heap `heap_push` / `heap_pop` / `heap_peek`.
- `src/parse.c`: strict argument validation with checked manual conversion
  (no `atoi` overflow).
- `src/log.c`: `log_format` formatter and `log_state` with one unbuffered
  `write(1, ...)` under `log_mutex`.
- `src/time.c`: `get_now_ms()` wrapper around `gettimeofday`.
- `src/init.c`: shared/array/dongle-mutex initialization, `dongles_ready`
  tracking, start timestamp and partial-init cleanup.
- `src/cleanup.c`: destroys shared/log/dongle mutexes and frees `shared.scratch`,
  heap, coders, dongles and `table`.
- `src/arbiter.c`: work-conserving grants, FIFO/EDF key snapshots, cooldown
  release, scratch-based priority pass.
- `src/dongle_pair.c`: canonical pair locking, pair grant, and release helpers.
- `src/coder.c`: compile → debug → refactor loop, interruptible phase waits,
  dongle acquisition/release, completion and stop handling.
- `src/monitor.c`: polling loop, burnout detection, finished-counter tracking.
- `src/main.c`: thread creation, monitor-first startup, join and cleanup.
- `tests/test_heap.c` + `tests/test_parse.c` + `tests/test_log.c` +
  `tests/test_init.c` + `tests/test_arbiter.c` + `tests/test_coder.c` +
  `tests/test_monitor.c` + `tests/test_dongle_pair.c`: 318/318 checks pass with
  `make test` (valgrind and TSAN clean).
- `tests/test_integration.py`: end-to-end log/timing/cooldown/burnout checks via
  `make test-integration`.
- `README.md`: required 42 documentation, concurrency analysis and AI disclosure.

Build status: `make` / `make re` now succeed. Per-dongle locking is implemented:
`shared.mutex` protects queue/counters/coder state, and each dongle mutex
protects its own holder/cooldown fields.

## Architectural constraints & rules

- **Thread model**: one thread per coder plus one monitor. The arbiter is code
  invoked under the table mutex, not a separate thread.
- **Norm compliance**: functions ≤ 25 lines, ≤ 4 parameters, ≤ 5 functions per
  file. C89 style (declarations at top of blocks). Use `static` for file-local
  helpers, include guards, no libft.
- **No global variables**: allocate one `t_table` in `main` and pass pointers
  (`&table->coders[i]` or `&table`) to threads.
- **Mutex separation**:
  - `shared.mutex` guards heap, scratch, stop flag, counters, `start_ms`,
    `arrival_counter`, and coder state read by the monitor.
  - Each `t_dongle.mutex` guards only that dongle's `holder` and
    `available_at`.
  - `log_mutex` guards stdout (`fd 1`).
  - Lock order: `shared.mutex` -> dongle mutexes in ascending id order.
  - Never hold a dongle mutex while waiting on a condition variable.
  - Never perform I/O while holding `shared.mutex`.
- **Clock & timing**: use `get_now_ms()` (`src/time.c`) for timestamps in
  milliseconds, not `clock_gettime`. Coder phases use `sleep_phase`, which
  builds an absolute `CLOCK_REALTIME` deadline from `get_now_ms() + duration`
  and `pthread_cond_timedwait`s on `stop_cond`; a concurrent `stop` wins after
  timeout. `usleep` (microseconds) is only for the monitor polling loop.
- **Logging**: no `printf`/`sprintf`/`snprintf`. Build each line in a local
  buffer with hand-rolled integer-to-string formatting, then emit it with a
  single unbuffered `write(1, ...)` under `log_mutex`.
- **Heap**: capacity is exactly `number_of_coders`; min-heap, smaller key wins,
  ties broken by smaller coder id. FIFO key = arrival counter, EDF key =
  `last_compile_start + time_to_burnout`, snapshotted at enqueue.
- **Arbiter**: work-conserving. Requests are processed in priority order; a
  request that cannot be granted is requeued **without reserving dongles**, so a
  lower request whose pair is currently free may still run. `arbiter_run` drains
  the heap into `table->shared.scratch`, broadcasts `shared.dongle_cond` when at
  least one request is granted, and calls `arbiter_try_pair` for each request.
  Pair locking, grant, and release helpers live in `src/dongle_pair.c`.
  `arbiter_request` and `arbiter_run` assume `shared.mutex` is already held;
  `arbiter_release` locks its dongles first, unlocks them, then acquires
  `shared.mutex` to run the arbiter and broadcast. `arbiter_release` broadcasts
  `shared.dongle_cond` unconditionally so waiters recompute cooldown/held state.
- **Condvars**: `shared.stop_cond` for `sleep_phase` phase waits,
  `shared.dongle_cond` for grant/cooldown waits; use
  `pthread_cond_broadcast` (not `pthread_cond_signal`).
- **Dongle mapping** (0-based, circular):
  `coder[i].left = dongle[(i + N - 1) % N]`, `coder[i].right = dongle[i]`.
- Join all threads before destroying mutexes/condvars and freeing memory.

## Commands

- `make test` — builds `test_codexion` from `tests/*.o` + the module objects,
  runs the unit suite.
- `make test-integration` — builds `codexion` and runs
  `tests/test_integration.py`.
- `make test-scenarios` — runs `tests/test_scenarios.sh`, a scenario matrix
  showing clean runs and burnout cases.
- `make` / `make re` — full build of the `codexion` executable.
- `make clean` / `make fclean`.
- No CI or external test framework. Tests use the `CHECK` macro in `tests/test.h`.
- `compile_flags.txt` is clangd-only; real flags live in the `Makefile`
  (`-Wall -Wextra -Werror -pthread -Iinclude`).
- Leak check (if valgrind is installed):
  `valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./codexion <args>`.
- Race check: no sanitizer target exists; add `-fsanitize=thread` manually for
  a debug build. The deliverable must compile with exactly the Makefile flags.

## Key edge cases to verify

- **1-coder / 1-dongle case**: single dongle acquired successfully without
  recursive-mutex or deadlock issues; pair helpers lock it once.
- **Deadlock freedom**: circular table with 2+ coders using atomic both-or-none
  requests and ascending dongle lock order.
- **Disjoint concurrency**: two coders with disjoint dongle pairs can release
  or compile without being serialized by the other pair.
- **Burnout precision**: monitor logs `<timestamp> X burned out` within 10 ms of
  the actual deadline breach.
- **Two exit paths**: clean completion when all coders hit
  `number_of_compiles_required`, vs. early abort on first burnout.
- **Invalid input**: specific error plus usage on `stderr` and exit code `1`.
- **Cooldown extremes**: `0` vs large values, with staggered contention.

## Repo gotchas

- New `src/*.c` files must be added to `SRCS` in the `Makefile`; test helpers
  must also be added to `TEST_OBJS`.
- `src/dongle_pair.c` holds the pair-lock helpers and is linked by both the
  executable and the unit-test binary.
- Build artifacts (`*.o`, `codexion`, `test_codexion`) are ignored; never commit
  them.
- The Makefile comment calling tests "standalone" is stale: heap tests already
  link `src/heap.o`.
