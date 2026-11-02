# Codexion Program Flow

This document describes how the main modules of Codexion interact at runtime.

## 1. Startup

`main` (`src/main.c`) performs four steps:

1. `parse_args` validates the eight mandatory arguments and fills `t_config`.
2. `init_table` allocates and initializes the shared `t_table`.
3. `run_simulation` creates the monitor and coder threads, then joins them.
4. `cleanup_table` destroys synchronization objects and frees memory.

Invalid arguments print an error and usage message to `stderr`; `main` returns
`1`.

## 2. Table initialization

`init_table` (`src/init.c`) allocates:

- `dongles[]`
- `coders[]`
- `shared.heap.items[]`
- `shared.scratch[]`

It initializes:

- `shared.mutex`
- `log_mutex`
- `shared.stop_cond`
- `shared.dongle_cond`
- every `dongles[i].mutex`

Then it records:

```c
table->shared.start_ms = get_now_ms();
```

`init_entries` initializes each dongle:

```c
id = i;
available_at = start_ms;
holder = -1;
```

and each coder:

```c
id = i + 1;
last_compile_start = start_ms;
compiles_done = 0;
finished = 0;
left  = &dongles[(i + N - 1) % N];
right = &dongles[i];
```

Every coder's first burnout deadline is therefore:

```text
start_ms + time_to_burnout
```

There is no startup barrier. The monitor is created first, then coders one by
one. The OS decides when each thread actually runs, so initial request arrival
order can vary.

## 3. Thread creation

`run_simulation` allocates `N + 1` thread IDs:

```text
threads[0]     monitor
threads[1..N]  coders
```

If a `pthread_create` call fails:

1. `stop_simulation` sets `stop = 1`.
2. It broadcasts `stop_cond` and `dongle_cond`.
3. It joins every thread that was created.
4. It frees the thread array and returns failure.

On success, all threads are joined before cleanup.

## 4. Shared synchronization state

`shared.mutex` protects:

- the heap and scratch array
- coder `last_compile_start`, `compiles_done`, and `finished`
- `shared.stop` and `shared.finished_counter`
- `shared.start_ms` and `shared.arrival_counter`

Each `dongles[i].mutex` protects only that dongle's `holder` and
`available_at`.

The pair-lock helpers in `src/dongle_pair.c` lock both required dongles in
ascending dongle-id order. For `N == 1`, the single dongle is locked once.

`log_mutex` protects output to file descriptor 1.

`shared.stop_cond` is used by coder phase waits.

`shared.dongle_cond` is used by coders waiting for dongles or cooldown.

## 5. Coder lifecycle

Each coder thread runs `coder_routine` (`src/coder.c`).

### Main loop

At the top of each iteration:

1. Lock `shared.mutex`.
2. If `stop`, unlock and exit.
3. If `compiles_done >= required`, set `finished = 1`, increment
   `finished_counter`, unlock and exit.
4. Otherwise unlock and run one cycle.

Completion is checked after a full compile/debug/refactor cycle.

If `required == 0`, the coder exits immediately without compiling.

### One cycle

`run_cycle` performs:

```text
acquire dongles
log "has taken a dongle" once or twice
log "is compiling"
wait time_to_compile
release dongles
increment compiles_done
log "is debugging"
wait time_to_debug
log "is refactoring"
wait time_to_refactor
```

### Acquiring dongles

`acquire_dongles`:

1. Locks `shared.mutex`.
2. Checks `stop`.
3. Calls `arbiter_request`.
4. Waits in `wait_for_dongles`.
5. Sets `last_compile_start = get_now_ms()`.
6. Unlocks.
7. Logs the dongle takes.

For `N == 1`, `left == right`, so only one take line is emitted.

### Waiting for dongles

`wait_for_dongles` is called with `shared.mutex` held.

It locks the two dongles briefly to inspect their holders and cooldowns, then
unlocks them before waiting.

It loops until both dongles have `holder == coder->id`, or until `stop` is set.

It waits on `shared.dongle_cond` until the later `available_at`. If that time is
already past, it uses a one-second fallback. On `ETIMEDOUT`, it calls
`arbiter_run` again.

`arbiter_release` locks its dongles, clears the holders, applies the cooldown,
unlocks the dongles, then acquires `shared.mutex` to run the arbiter and
broadcast `shared.dongle_cond`.

### Phase waits

`sleep_phase` builds an absolute `CLOCK_REALTIME` deadline from:

```c
get_now_ms() + duration
```

It uses `pthread_cond_timedwait` on `shared.stop_cond` with `shared.mutex`. If
the monitor sets `stop`, the coder wakes immediately and exits.

After a phase returns normally, the coder calls `stop_requested` before logging
the next state or releasing dongles. This prevents a state transition from being
logged after the burnout line.

## 6. Arbiter lifecycle

The arbiter is code, not a separate thread. It runs under `shared.mutex`.

### Request

`arbiter_request`:

- creates a `t_request`
- snapshots the priority key:
  - FIFO: `arrival_counter++`
  - EDF: `last_compile_start + time_to_burnout`
- pushes the request into the heap
- calls `arbiter_run`

### Run

`arbiter_run`:

1. Pops requests from the heap in priority order.
2. For each request:
   - grants both dongles if they are grantable
   - otherwise pushes the request back without reserving dongles
3. Broadcasts `dongle_cond` if at least one request was granted.

The heap orders requests by:

```text
smaller key first
equal keys -> smaller coder id
```

### Release

`arbiter_release`:

1. Sets both dongles to `holder = -1`.
2. Sets `available_at = now + dongle_cooldown`.
3. Runs `arbiter_run`.
4. Broadcasts `dongle_cond` unconditionally.

The unconditional broadcast is important because releasing dongles changes the
state even if no request was granted.

### Work-conserving behavior

A blocked request does not reserve dongles.

A lower-priority request:

- waits if its own dongles are held or cooling
- may proceed if its pair is currently free

This avoids a reservation convoy and keeps the table live with many coders.

## 7. Monitor lifecycle

The monitor runs `monitor_routine` (`src/monitor.c`).

Every millisecond it calls `check_state`.

`check_state` locks `shared.mutex` and returns:

- `-1` if `stop` is already set or `finished_counter == N`
- a coder id if someone burned out
- `0` otherwise

`find_burned` checks coders that:

- are not finished
- have not reached the required compile count
- satisfy:

```c
now > last_compile_start + time_to_burnout
```

On burnout:

1. `stop = 1`
2. broadcast `stop_cond`
3. broadcast `dongle_cond`
4. unlock
5. log `"X burned out"`
6. exit

The burnout line is therefore emitted after stopping the simulation but within
the required 10 ms window.

When all coders finish, the monitor sees:

```c
finished_counter == number_of_coders
```

and exits without setting `stop`.

## 8. Logging

Every message goes through `log_state` (`src/log.c`).

It:

1. locks `log_mutex`
2. computes `get_now_ms() - start_ms`
3. formats the line with hand-written integer conversion
4. writes the complete line with one `write`
5. unlocks

This guarantees complete lines, no interleaving, no stdio buffering, and
timestamp ordering matching log ordering. No logging happens while holding
`shared.mutex`.

## 9. Exit paths

### Clean completion

Each coder completes its required cycles, logs its final `"is refactoring"`,
marks itself finished, and increments `finished_counter`.

The monitor sees the counter reach `N`, exits, and `main` joins all threads.

The last log line is a refactoring line. There is no completion message.

### Burnout

The monitor sets `stop` and broadcasts both condition variables.

- Coders in `sleep_phase` wake on `stop_cond` and exit.
- Coders in `wait_for_dongles` wake on `dongle_cond` and exit.
- A coder that was compiling exits without releasing its dongles; the
  simulation is ending, so cleanup follows after joins.

The monitor logs the single burnout line and exits.

### Thread creation failure

`main` sets `stop`, broadcasts both condition variables, joins created threads,
frees the thread array, cleans up the table, and returns `1`.

## 10. Interaction summary

A normal compile request flows like this:

```text
coder
  -> locks shared.mutex
  -> arbiter_request
       -> creates request with FIFO/EDF key
       -> heap_push
       -> arbiter_run
            -> grants free pairs or requeues blocked requests
            -> broadcasts dongle_cond if a grant happened
  -> wait_for_dongles
       -> sleeps on dongle_cond
  -> wakes when holders == coder id
  -> sets last_compile_start
  -> unlocks shared.mutex
  -> logs "has taken a dongle" x1/x2
  -> logs "is compiling"
  -> sleep_phase(compile)
  -> locks shared.mutex
  -> arbiter_release
       -> frees dongles + cooldown
       -> arbiter_run
       -> broadcasts dongle_cond
  -> unlocks
  -> logs "is debugging"
  -> sleep_phase(debug)
  -> logs "is refactoring"
  -> sleep_phase(refactor)
  -> loop
```

Meanwhile, independently:

```text
monitor
  -> every 1 ms: lock shared.mutex
  -> check finished_counter and every unfinished coder's deadline
  -> if burnout: stop = 1, broadcast stop_cond + dongle_cond, unlock
  -> log "X burned out"
  -> exit
```

Coders request and release resources, the arbiter grants free pairs, the monitor
watches deadlines and completion, and the logger serializes all visible output.
