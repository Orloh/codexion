# AGENTS.md — Codexion Development Playbook

42 "codexion": a C/pthreads simulation where coder threads compete for dongles.
`summary.md` is the authoritative design/spec — read it before implementing.

## Current status & build order

Implemented:
- `include/codexion.h`: shared structs, `t_policy` (FIFO/EDF), heap/parse/log
  API.
- `src/heap.c`: min-heap `heap_push` / `heap_pop` / `heap_peek`.
- `src/parse.c`: strict argument validation with checked manual conversion
  (no `atoi` overflow).
- `src/log.c`: `log_format` formatter and `log_state` with one unbuffered
  `write(1, ...)` under `log_mutex`.
- `src/init.c`: simulation/array allocation, sync-object init, dongle/coder
  table setup, start timestamp and partial-init cleanup.
- `src/cleanup.c`: destroys sync objects and frees `scratch`, heap, coders,
  dongles and `sim`.
- `tests/test_heap.c` + `tests/test_parse.c` + `tests/test_log.c` +
  `tests/test_init.c`: 218/218 checks pass with `make test` (valgrind clean).

Missing (listed in `Makefile` but no file exists):
- `src/main.c`, `arbiter.c`, `coder.c`, `monitor.c`.
- Therefore `make` / `make re` currently fails with
  `No rule to make target 'src/main.o'`.
- `README.md` (required by the subject) does not exist.

Implementation roadmap (suggested build order; full build still needs step 4):
1. `src/arbiter.c`: atomic both-or-none grab, heap integration, cooldown checks,
   reservation-aware grants.
2. `src/coder.c`: compile → debug → refactor loop, interruptible phase waits via
   `pthread_cond_timedwait`.
3. `src/monitor.c`: polling loop (≤ 10 ms precision), burnout detection,
   finished-counter tracking.
4. `src/main.c`: entry point tying init, thread creation, joining and cleanup
   together.
5. `README.md`: required documentation with AI-usage disclosure and concurrency
   analysis.

## Architectural constraints & rules

- **Thread model**: one thread per coder plus one monitor. The arbiter is code
  invoked under the table mutex, not a separate thread.
- **Norm compliance**: functions ≤ 25 lines, ≤ 4 parameters, ≤ 5 functions per
  file. C89 style (declarations at top of blocks). Use `static` for file-local
  helpers, include guards, no libft.
- **No global variables**: allocate one `t_sim` in `main` and pass pointers
  (`&sim->coders[i]` or `&sim`) to threads.
- **Mutex separation**:
  - `table_mutex` guards shared simulation state (dongles, reservations, heap,
    scratch, stop flag, counters).
  - `log_mutex` guards stdout (`fd 1`).
  - Never perform I/O while holding `table_mutex`.
- **Clock & timing**: use `gettimeofday` for timestamps (converted to ms), not
  `clock_gettime`. Use `pthread_cond_timedwait` with an absolute
  `CLOCK_REALTIME` timespec for interruptible sleeps and cooldown waits.
  `usleep` takes microseconds.
- **Logging**: no `printf`/`sprintf`/`snprintf`. Build each line in a local
  buffer with hand-rolled integer-to-string formatting, then emit it with a
  single unbuffered `write(1, ...)` under `log_mutex`.
- **Heap**: capacity is exactly `number_of_coders`; min-heap, smaller key wins,
  ties broken by smaller coder id. FIFO key = arrival counter, EDF key =
  `last_compile_start + time_to_burnout`, snapshotted at enqueue.
- **Arbiter**: reservation-aware. A blocked request reserves its dongles
  (`t_dongle.reserved_by`); lower requests sharing them wait, while fully
  disjoint pairs may still run. `arbiter_run` drains the heap into
  `sim->scratch` to process requests in priority order.
- **Condvars**: `stop_cond` for phase waits, `dongle_cond` for grant/cooldown
  waits; use `pthread_cond_broadcast` (not `pthread_cond_signal`).
- **Dongle mapping** (0-based, circular):
  `coder[i].left = dongle[(i + N - 1) % N]`, `coder[i].right = dongle[i]`.
- Join all threads before destroying mutexes/condvars and freeing memory.

## Commands

- `make test` — builds `test_codexion` from `tests/*.o` + `src/heap.o` +
  `src/parse.o` + `src/log.o` + `src/init.o` + `src/cleanup.o`, runs it.
- `make` / `make re` — full build; only works after the missing sources exist.
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
  recursive-mutex or deadlock issues.
- **Deadlock freedom**: circular table with 2+ coders using atomic both-or-none
  requests.
- **Burnout precision**: monitor logs `<timestamp> X burned out` within 10 ms of
  the actual deadline breach.
- **Two exit paths**: clean completion when all coders hit
  `number_of_compiles_required`, vs. early abort on first burnout.
- **Invalid input**: specific error plus usage on `stderr` and exit code `1`.
- **Cooldown extremes**: `0` vs large values, with staggered contention.

## Repo gotchas

- New `src/*.c` files must be added to `SRCS` in the `Makefile`.
- Build artifacts (`*.o`, `codexion`, `test_codexion`) are ignored; never commit
  them.
- The Makefile comment calling tests "standalone" is stale: heap tests already
  link `src/heap.o`.
