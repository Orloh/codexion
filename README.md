*This project has been created as part of the 42 curriculum by orhernan.*

# Codexion

## Description

Codexion is a C concurrency simulation built with POSIX threads. Several coders
sit around a circular table with one USB dongle between each pair of neighbours.
A coder must hold both the left and right dongle simultaneously in order to
compile. After compiling, the coder releases both dongles and moves through
debugging and refactoring before trying to compile again.

The simulation stops when every coder has compiled the required number of times,
or when a coder misses its burnout deadline. A separate monitor thread detects
burnout and stops the simulation. Dongle access is controlled by a
reservation-aware arbiter using a priority heap:

- `fifo`: requests are served in arrival order.
- `edf`: requests are served by earliest burnout deadline.

The project focuses on thread synchronization, condition variables, deadlock
prevention, starvation prevention, cooldown handling, and precise timing.

## Instructions

Build the executable with:

```sh
make
```

Run it with the eight mandatory arguments:

```sh
./codexion number_of_coders time_to_burnout time_to_compile time_to_debug \
    time_to_refactor number_of_compiles_required dongle_cooldown fifo|edf
```

Example:

```sh
./codexion 5 2000 200 200 200 3 50 edf
```

Unit tests:

```sh
make test
```

End-to-end integration tests:

```sh
make test-integration
```

Clean build artifacts:

```sh
make clean
make fclean
```

## Resources

- POSIX threads documentation: `man pthread_create`, `man pthread_join`,
  `man pthread_mutex_init`, `man pthread_cond_init`, and
  `man pthread_cond_timedwait`.
- POSIX synchronization concepts: mutexes, condition variables, condition
  broadcasts, and timed waits.
- The dining philosophers problem, as a classic model of circular resource
  contention and deadlock.
- Earliest Deadline First (EDF) scheduling and priority-queue data structures.
- `gettimeofday(2)` and POSIX realtime clock semantics for millisecond timing.

### AI usage

AI was used for guided planning and design discussions, explanation of POSIX
threading concepts, validation of the concurrency model, test-plan design, and
review of the implementation. The code and design decisions were reviewed and
are understood by orhernan, who remains responsible for the submitted work.

## Blocking cases handled

### Deadlock prevention

Coders never hold one dongle while waiting for the other. A request asks for the
left and right dongle as one atomic pair. The arbiter either grants both or
grants neither, so a coder waiting for dongles holds nothing.

This removes hold-and-wait and circular wait from the four Coffman conditions.
Mutual exclusion and no-preemption remain, as required by the subject.

### Starvation prevention

The arbiter uses strict priority with reservations:

- A blocked higher-priority request reserves its needed dongles.
- Lower-priority requests that share a reserved dongle wait.
- Lower-priority requests with fully disjoint dongle pairs may still proceed.

Under feasible EDF parameters, the earliest-deadline coder is not overtaken by
later-deadline requests for the same dongles. FIFO uses the same reservation
rule with arrival order.

### Cooldown handling

When a coder releases its dongles, each dongle receives:

```c
available_at = get_now_ms() + dongle_cooldown;
```

A dongle is grantable only when it is free and `now >= available_at`. Waiters
use `pthread_cond_timedwait` until the relevant cooldown expires, then rerun the
arbiter.

### Burnout detection

Each coder updates `last_compile_start` when it actually starts compiling. The
monitor polls every millisecond and checks unfinished coders:

```c
now > last_compile_start + time_to_burnout
```

On the first burnout, the monitor logs the event, sets the global stop flag, and
broadcasts both condition variables. The burnout line is emitted within the
required 10 ms window, subject to normal OS scheduling jitter.

### Log serialization

All coder and monitor messages are built in a local buffer and emitted with one
unbuffered `write(1, ...)` call under `log_mutex`. A line therefore cannot be
interleaved with another line. The timestamp is computed inside the logging
lock, so output order matches timestamp order.

## Thread synchronization mechanisms

### `shared.mutex`

`shared.mutex` protects the global simulation state:

- the priority heap and scratch array,
- coder compile counters, `last_compile_start`, and finished state,
- the stop flag and `finished_counter`,
- `start_ms` and the FIFO arrival counter.

No logging is performed while holding `shared.mutex`.

### Per-dongle mutexes

Each `t_dongle` has its own `pthread_mutex_t`. That mutex protects only that
dongle's `holder`, `available_at`, and `reserved_by`.

The pair-lock helpers in `src/dongle_pair.c` lock both required dongles in
ascending dongle-id order. This makes the both-or-none grant atomic and prevents
circular-wait deadlocks. For `N == 1`, the single unique dongle is locked once.

`arbiter_release` locks its two dongles, clears the holders, applies the
cooldown, unlocks the dongles, and only then acquires `shared.mutex` to run the
arbiter. This allows disjoint releases to proceed independently.

### `log_mutex`

`log_mutex` protects file descriptor 1. It is separate from `shared.mutex` so
slow output does not block the simulation and so the monitor can preserve its
burnout precision.

### `shared.stop_cond`

`sleep_phase` uses `pthread_cond_timedwait` on `shared.stop_cond` with an
absolute `CLOCK_REALTIME` deadline. This makes compile, debug, and refactor waits
interruptible: when the monitor sets `shared.stop`, sleeping coders wake
immediately instead of waiting for their full phase duration.

### `shared.dongle_cond`

Dongle waiters sleep on `shared.dongle_cond`. The arbiter broadcasts it when
dongles are granted or released. A coder that wakes always re-checks whether
both of its dongles now have `holder == its id`; spurious wakeups are harmless.

Waiters hold `shared.mutex` for the condition wait but lock the dongles only
briefly to inspect their state, then unlock them before waiting.

Cooldown waiters use a timed wait at `available_at`. When it expires, they rerun
`arbiter_run` because no thread releases a dongle at the moment a cooldown ends.

### Monitor coordination

The monitor takes a consistent snapshot under `shared.mutex`, checks deadlines,
and skips finished coders and coders that have already reached the required
compile count. On burnout it sets `shared.stop` and broadcasts
`shared.stop_cond` and `shared.dongle_cond`. Coders then leave their waits and
exit without incrementing `finished_counter`.

### Example race prevention

Without a mutex, two coders could both observe the same dongle as free and both
mark themselves as its holder. In Codexion, the check, reservation, and grant
for a pair happen while both dongle mutexes are held, so only one coder can be
granted a given dongle. The log mutex similarly prevents two threads from mixing
their output lines.
