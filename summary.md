# Codexion — Solution Plan (working summary)

42 project: a concurrency simulation where coding threads compete for a limited
number of dongles. Threads, mutexes and dongle arbitration are the core topics.

> This document is built incrementally. Each section is written only after the
> corresponding topic has been understood and validated.

## Physical model

- Each **coder is a thread** that cycles through three mutually exclusive tasks
  and repeats forever:
  **compile → debug → refactor → compile → …**
- Each task takes its configured duration in milliseconds
  (`time_to_compile`, `time_to_debug`, `time_to_refactor`).
- Every coder has a **burnout deadline** equal to
  `last_compile_start + time_to_burnout`. For the first cycle it is anchored at
  the start of the simulation.
  If a coder does **not start compiling before its deadline**, it **burns out**
  and the whole simulation stops. The burnout is the consequence of missing the
  deadline, not something that is waited for.
- To compile, a coder must hold its **left and right dongle simultaneously**.
  When compiling finishes it **releases both** dongles and starts debugging.
  After refactoring it **immediately tries to acquire dongles again**.
- There are **as many dongles as coders**. Coders sit at a **circular table**:
  coder `N` sits between `N-1` and `N+1`, and coder `1` sits next to coder
  `number_of_coders`. Therefore **each dongle is shared by exactly two adjacent
  coders** (coder `N`'s right dongle is coder `N+1`'s left dongle). This
  adjacency is the source of the deadlock risk.
- A dongle is a **resource**; its state is guarded by a **mutex**
  (`pthread_mutex_t`), and a condition variable (`pthread_cond_t`) may manage its
  waiting queue. The mutex protects access; it is not the dongle itself.
- After a dongle is released it has a **cooldown** (`dongle_cooldown` ms) during
  which nobody may take it.
- **Coders do not communicate** with each other and cannot see whether another
  coder is close to burning out.
- The **monitor** is a separate thread whose job is to detect burnout precisely
  and stop the simulation. Its burnout log must be printed within **10 ms** of
  the real moment of burnout.
- The `scheduler` argument is a **dongle arbitration policy**, not a thread.
  It is `fifo` or `edf`:
  - `fifo`: the dongle is granted to the coder whose request arrived first.
  - `edf`: the dongle is granted to the coder with the earliest deadline
    (`last_compile_start + time_to_burnout`).
- **One queue abstraction, two policies**: a priority queue (heap) can serve
  both — `fifo` = priority by arrival order, `edf` = priority by deadline. Two
  separate structures are not strictly required.
- **Two stop conditions**: the simulation ends when someone **burns out**, or
  when **all coders have compiled at least `number_of_compiles_required`
  times**.
- **1-coder special case**: there is exactly one dongle, which is both the left
  and right dongle of the single coder. It must be **acquired once** (no
  recursive mutex needed).

## Blocking cases handled

### The deadlock problem (Coffman conditions)

At a circular table, if every coder grabs its **left** dongle first and then
waits for its **right**, the ring closes into a **deadlock**: each coder holds
the dongle its neighbor needs and waits forever. With 2 coders the cycle is
immediate, but the same applies to the whole ring.

The four **Coffman conditions** and their fate here:

| Condition | Present? | Removable? |
| --- | --- | --- |
| **Mutual exclusion** — a dongle is used by only one coder at a time | yes | **inherent** (required by the rules) |
| **Hold-and-wait** — hold one dongle while requesting another | yes | **removable** |
| **No preemption** — dongles cannot be seized by force | yes | **inherent** (only the holder releases after compiling) |
| **Circular wait** — a closed chain of coders each waiting on the next | yes | **removable** |

Deadlock requires **all four**. Since mutual exclusion and no-preemption are
dictated by the subject, the solution removes the other two.

### The chosen mechanism: atomic grab (both-or-none)

A coder requests **both** its dongles together and proceeds **only when both can
be granted at once**; otherwise it waits holding **nothing**.

- Because it never holds one dongle while waiting for another, **hold-and-wait**
  disappears.
- Because no coder can be the holder in a wait cycle, **circular wait**
  disappears.
- The coder then compiles and releases both dongles for its neighbors.

(Imposing a global order on dongles is an equivalent alternative, but the atomic
both-or-none request matches the required scheduler arbitration.)

### Starvation and the scheduler

- **FIFO** (first-in, first-out) can **starve** a coder without any deadlock: a
  coder that keeps arriving late can be overtaken indefinitely, eventually
  missing its deadline.
- **EDF** (earliest deadline first) prioritizes the coder closest to burnout
  (`last_compile_start + time_to_burnout`), so waiting is bounded ⇒ **no
  starvation**, provided the parameters are viable. This is why the mandatory
  part states nobody should burn out under `edf` when parameters are viable.

### Division of labor

- **Arbitration / scheduler** (atomic grab + FIFO/EDF) **prevents** deadlock and
  starvation.
- **Burnout detection** does **not** prevent anything: it is the **safety net**
  that reports failure and stops the simulation within **10 ms** of the real
  burnout moment.

## Monitor & timing

### Shared state

Each coder owns its own `last_compile_start` and writes it itself (single writer)
when it **starts** compiling. The monitor only reads it. The shared fields are
protected by a mutex so the monitor gets a consistent snapshot:

- `last_compile_start` (one per coder),
- a `finished` flag (one per coder),
- a `finished_counter` (total coders done).

The coder, right before exiting, sets its `finished` flag and increments
`finished_counter`, both under the same mutex. Using one mutex for all three
fields keeps the monitor's snapshot atomic.

### Burnout check

The monitor compares, for each **unfinished** coder:

```
now > last_compile_start + time_to_burnout   →   burnout
```

Finished coders are **skipped**, otherwise their frozen `last_compile_start`
would produce a false burnout. On the first burnout the monitor logs
`<timestamp> X burned out` (within 10 ms), sets the global `stop` flag and
broadcasts the stop condition; it then stops checking.

### Monitor loop (shape)

```
while true:
    lock; snapshot coders (last_compile_start, finished) + counter; unlock
    if counter == number_of_coders: break        # all done
    for each unfinished coder:
        if now > last_compile_start + time_to_burnout:
            log "X burned out"
            lock; stop = 1
            broadcast(stop_cond)
            broadcast(dongle_cond)
            unlock
            break
    sleep a little (well under 10 ms)
```

Polling must be **under 10 ms** to meet the log requirement, but not so hot that
it hogs the CPU — around 1 ms is a good balance.

### Clock

- Only **`gettimeofday`** is authorized (`clock_gettime` is not in the allowed
  list).
- Convert to milliseconds: `tv.tv_sec * 1000 + tv.tv_usec / 1000`.
- Take **one start reference** at simulation start; log `now_ms - start_ms`, so
  the first log is `0`.
- `usleep` takes **microseconds** (`n` ms → `usleep(n * 1000)`).
- `pthread_cond_timedwait` needs an **absolute** `struct timespec` on
  `CLOCK_REALTIME` (same epoch as `gettimeofday`), built from the current time
  plus the desired duration.

### Stopping the simulation

- Both stop paths (burnout, or all coders finished) go through the same shared
  `stop` flag protected by `table_mutex`. There are two condition variables:
  `stop_cond` for interruptible phase waits and `dongle_cond` for dongle and
  cooldown waits.
- The monitor can only wake threads blocked on a condition variable. On stop it
  therefore sets `stop` and broadcasts **both** `stop_cond` and `dongle_cond`.
- Chosen solution: make the phase waits **interruptible with
  `pthread_cond_timedwait`** on `stop_cond`, in a loop that re-checks `stop`.
  `ETIMEDOUT` (from `<errno.h>`) means the full duration elapsed; a normal
  return means `stop` was set and the coder should exit. This also gives better
  time precision and no `usleep` drift.
- Dongle waiters use `dongle_cond` with the same rule: after waking they
  re-check `stop` and exit instead of acquiring dongles.

```
lock(&table_mutex);
abstime = now + duration;
while (!stop)
    if (pthread_cond_timedwait(&stop_cond, &table_mutex, &abstime) == ETIMEDOUT)
        break;                    /* full duration elapsed */
unlock(&table_mutex);
```

### Two exit paths

- **All done (self-termination)**: each coder exits its loop when its own
  compile count reaches `number_of_compiles_required` (`>=`, since it starts at
  0), marking itself finished. The monitor notices `finished_counter` reaching
  `number_of_coders` and exits.
- **Burnout**: the monitor sets `stop` and broadcasts, so sleeping and waiting
  coders wake and exit early; `main` then `pthread_join`s every thread and frees
  all memory (no leaks).

## Dongle arbitration & cooldown

### Shared state without global variables

No file-scope variables. In `main`, allocate **one heap struct** holding all
shared state and pass a pointer to every thread via `pthread_create`'s argument:

- `config` (the 8 parsed arguments),
- `dongles[]` (each: `id`, `available_at`, `holder`, `reserved_by`),
- `coders[]` (each: `id`, `last_compile_start`, `finished`, `compiles_done`),
- the waiting **priority queue**,
- a temporary **scratch array** used to process the queue in priority order,
- `pthread_mutex_t` guarding the whole table,
- `stop` flag + `stop_cond` / `dongle_cond`, `finished_counter`.

Each coder thread additionally receives its own index/id so it can find "its"
dongles and coder slot.

### Locking model

Chosen first implementation: **one global mutex** protecting dongle states,
cooldowns, reservations and the queue. A **single global priority queue** holds
the waiting requests (a coder waiting for a *pair* is one request). Every
request is enqueued; a central reservation-aware `arbiter_run` decides grants
under the mutex. A blocked request keeps its dongles reserved, so lower requests
sharing them wait, while lower requests with fully disjoint pairs may still be
granted.

The **arbiter is code, not a thread** — it runs under the mutex when a coder
requests, when a coder releases, and when a cooldown timeout expires. The
**only extra thread is the monitor**.

### Request and dongle state

- `dongle`: `{ id, available_at, holder, reserved_by }`; `holder == -1` means
  free and `reserved_by == -1` means unreserved. A dongle is **grantable** for a
  request when `holder == -1 && now >= available_at` and it is not reserved for
  another coder.
- `request`: coder id + the **priority key snapshotted at enqueue time**:
  - `fifo` → an **arrival counter** (order of request),
  - `edf`  → `last_compile_start + time_to_burnout` evaluated when the request is
    made.

### Grant flow (atomic both-or-none, reservation-aware)

A coder that wants to compile, under the table mutex:
1. Enqueue one request with its snapshotted priority key.
2. Run `arbiter_run`; wait on `dongle_cond` until granted or `stop`. It holds
   nothing while waiting — no partial grab.

When a coder **releases** after compiling, still under the mutex:
1. Mark both dongles **free**, clear `holder`, set
   `available_at = now + dongle_cooldown`.
2. Run `arbiter_run`; it broadcasts `dongle_cond` if it grants anything.
3. Unlock.

`arbiter_run` (called under `table_mutex`):
1. Clear every `dongle.reserved_by` to `-1`.
2. Pop all queued requests into `sim->scratch`; this yields them in priority
   order.
3. Walk `scratch` from highest to lowest priority:
   - if both unique dongles are grantable and neither is reserved, grant both
     (set `holder`) and **do not push it back**;
   - otherwise reserve each still-unreserved dongle for this coder and push the
     request back into the heap.
4. Broadcast `dongle_cond` once if at least one request was granted.

For `N == 1`, `left == right`: check, reserve, grant and release one unique
dongle — never count the same pointer twice. Use `pthread_cond_broadcast` (not
`signal`); waiters re-check whether their dongles are now held by them.

The grant is **pushed** by the arbiter; waiters don't poll.

### Cooldown waiting

A dongle that is held cannot be taken; the waiter waits on `dongle_cond` for a
release/stop broadcast. A dongle that is free but still cooling cannot be
granted either: when both needed dongles are free and cooling, use
**`pthread_cond_timedwait` on `dongle_cond` with an absolute timeout at the
later `available_at`**. On `ETIMEDOUT`, no thread releases at that moment, so
the waiter must run `arbiter_run` before re-checking. Any early wake (grant,
release, or stop) just loops. This is why the interruptible-wait primitive
chosen for phase timing is reused here.

### Fairness: reservation-aware strict priority (chosen)

The chosen policy is **reservation-aware** (see `arbiter_run` above). The
subject guarantees **nobody burns out under `edf` when parameters are viable**,
so the EDF path uses **strict priority**: a dongle is **reserved** for the
earliest-deadline contender and is **not** given to a lower-priority request,
even if it must idle until the high coder's other dongle frees. Work-conserving
hand-out without reservations could let a lower request take a shared dongle and
starve the urgent coder into burnout. `fifo` uses the same reservation rule with
arrival order.

Serving **disjoint** pairs (different dongles) never delays a higher-priority
coder, so concurrency across independent pairs is preserved. This is why the
arbiter scans the whole queue in priority order instead of stopping at the first
blocked request. The distinction is only observable with `N >= 4`: for `N <= 3`,
every pair of coders shares at least one dongle.

### Relation to "coders don't communicate"

The arbiter reading each request's deadline key is **policy-driven resource
management**, exactly what the subject prescribes for EDF. Coders never observe
or message each other; the dongle grants by rule. No rule is broken.

## Logging

### Messages

Coder emits four **distinct** messages (`N > 1`: five lines per cycle, since the
dongle take is logged once per distinct dongle; `N == 1`: four lines per cycle,
because `left == right`):

- `timestamp_in_ms X has taken a dongle` — once **per distinct dongle**, at the
  moment it is taken (`N > 1`: **two** lines; `N == 1`: **one** line).
- `timestamp_in_ms X is compiling` — when compiling starts.
- `timestamp_in_ms X is debugging` — when debugging starts.
- `timestamp_in_ms X is refactoring` — when refactoring starts.

Monitor emits:

- `timestamp_in_ms X burned out` — when a burnout is detected.

### Output: `write`, not `printf`

`printf`/stdio buffering causes problems: when stdout is redirected (a file or
pipe, as the grader captures it) it becomes **fully buffered** and flushes late,
so timestamps look out of order and the burnout line may not be visible within
10 ms. Instead use a **single `write(1, ...)` per message**, which is unbuffered
and (for lines under `PIPE_BUF`) atomic.

Consequence: the line must be built in a **buffer first**. Since `snprintf` /
`sprintf` are **not authorized**, the integer→string conversion must be
**hand-rolled** with a small helper (allowed functions only).

### Serialization

- A **dedicated logging mutex**, separate from the table mutex, guards **stdout /
  fd 1**. One guard for one resource.
- The **timestamp computation AND the `write`** both happen **inside** the
  logging lock, so two threads cannot compute timestamps and then print out of
  order.
- Emit each message with exactly **one `write`** of the full line (including
  `\n`) so lines never split.

### Two distinct resources

- **Table mutex** — protects the shared simulation state (dongles, coder state,
  cooldowns, priority queue, stop flag).
- **Logging mutex** — protects the output stream (fd 1).

They are kept separate because I/O is slow and must not block the simulation,
because sharing one lock would hurt the monitor's 10 ms precision, and because
separate locks avoid a table-vs-log lock-ordering deadlock. Rule: **never do I/O
while holding the table mutex** — gather what you need, release, then log.

### Burnout precision

The monitor takes `now` and writes `X burned out` immediately (same logging
mutex) using the same `now_ms - start_ms` format; because it polls well under
10 ms the line lands within the required window.

## Argument parsing & validation

### Arguments (in order)

`number_of_coders time_to_burnout time_to_compile time_to_debug time_to_refactor
number_of_compiles_required dongle_cooldown scheduler`

The program expects **exactly 8 arguments** (so `argc == 9`). All are mandatory.

### Rules

- **`number_of_coders` ≥ 1** (the single-coder / single-dongle case is valid).
- **`scheduler`** must be exactly `fifo` or `edf` (compare with `strcmp`).
- The five **time values** (`time_to_burnout`, `time_to_compile`,
  `time_to_debug`, `time_to_refactor`, `dongle_cooldown`) are in milliseconds and
  must be **integers ≥ 0**. Zero is accepted: a zero duration means the phase is
  skipped, and `dongle_cooldown = 0` means a released dongle is immediately
  available. (Edge case to keep in mind: `time_to_burnout = 0` makes the deadline
  essentially immediate.)
- Reject **negative** values, **non-integer** values, and any **malformed** input.

### `atoi` is not enough

`atoi` reports no errors and silently accepts bad input:

- trailing garbage → `"12abc"` → `12`,
- leading whitespace / signs → `"  +5"` → `5`,
- empty string → `0` (indistinguishable from a real `0`),
- overflow → undefined behavior.

So after `atoi`, validate the **raw string**: non-empty, and every character is a
digit (`'0'`..`'9'`). Bound the digit count to avoid overflow. `fifo`/`edf` via
`strcmp`.

### On invalid input

- Print a **usage message plus the specific error** to **stderr**
  (`fprintf(stderr, ...)`).
- **`return (1);`** from `main` (non-zero = failure); `0` on a clean run.

### Storage

Parsed values live in a **`config` struct** (fields named like the arguments) that
is allocated in `main` and reached by pointer from every thread — consistent with
the no-globals rule.

## Data structures

No file-scope variables. `main` allocates **one shared simulation struct** (the
"table") and each thread receives a pointer into it. Struct sketch:

```c
typedef struct s_sim t_sim;            /* forward declaration */

typedef struct s_config {
    int   number_of_coders;
    long  time_to_burnout;
    long  time_to_compile;
    long  time_to_debug;
    long  time_to_refactor;
    int   number_of_compiles_required;
    long  dongle_cooldown;
    int   policy;                      /* FIFO or EDF (parsed from the string) */
} t_config;

typedef struct s_dongle {
    int   id;
    long  available_at;                /* ms when cooldown ends (drop time) */
    int   holder;                      /* coder id, or -1 if free */
    int   reserved_by;                 /* coder id, or -1; transient */
} t_dongle;

typedef struct s_coder {
    t_sim    *sim;                     /* back-pointer to shared state */
    int       id;                      /* 1 .. number_of_coders */
    long      last_compile_start;      /* anchors the burnout deadline */
    int       compiles_done;
    int       finished;                /* whole sim done; monitor skips it */
    t_dongle *left;
    t_dongle *right;
} t_coder;

typedef struct s_request {
    t_coder *coder;
    long     key;                      /* fifo: arrival counter; edf: deadline */
} t_request;

typedef struct s_heap {
    t_request *items;                  /* array, capacity = number_of_coders */
    int        size;
} t_heap;

typedef struct s_sim {
    t_config        config;
    t_dongle       *dongles;           /* number_of_coders entries */
    t_coder        *coders;            /* number_of_coders entries */
    t_heap          heap;
    t_request      *scratch;           /* capacity = number_of_coders */
    pthread_mutex_t table_mutex;       /* guards all of the above */
    pthread_mutex_t log_mutex;         /* guards fd 1 (see Logging) */
    int             stop;              /* set by monitor */
    pthread_cond_t  stop_cond;         /* phase waits */
    pthread_cond_t  dongle_cond;       /* grant/cooldown waits */
    int             finished_counter;
    long            start_ms;
    long            arrival_counter;   /* monotonic, for FIFO keys */
} t_sim;
```

### Notes

- **`config` is immutable** parsed input; `start_ms` and the arrival counter are
  **runtime state** and live in `t_sim`, not in `config`.
- **`dongle`**: `holder == -1` means free; grantable = `holder == -1 &&
  now >= available_at && reserved_by == -1`.
- **`dongle.reserved_by`** is transient: it is reset at the start of every
  `arbiter_run` and prevents lower-priority requests from taking a dongle that a
  higher-priority blocked request needs.
- **`scratch`** is an array of `number_of_coders` requests used only while the
  arbiter pops the heap in priority order; it is not part of the waiting queue.
- **`coder`**: references its dongles by **pointer** (stable addresses). The
  `finished` flag means "the whole simulation is done for this coder" — not
  "finished compiling" — and tells the monitor to skip it.
- **`request.key`** is snapshotted **at enqueue time**: `fifo` = the global
  `arrival_counter`, `edf` = `last_compile_start + time_to_burnout`.
- **`heap`** capacity is exactly `number_of_coders` (≤ one outstanding request per
  coder). Details in the Heap section.
- **Tie-break** equal keys by **coder id**.

### Thread argument (Option B)

Each thread receives `&sim->coders[i]` (a stable address in the coder array) and
casts it to `t_coder *`; from `self->sim` it reaches all shared state. No
per-thread heap allocation is needed. The circular dependency (table ↔ coder) is
resolved by the single forward declaration `typedef struct s_sim t_sim;`.

### Ownership

`main` allocates `sim`, `dongles[]`, `coders[]`, the heap array and the scratch
array, initializes the mutexes and both condition variables, creates the coder +
monitor threads, `pthread_join`s them, then **destroys** the mutexes/condvars and
**frees** all allocations. No other thread allocates or frees.

## Priority queue (heap)

The mandatory part requires implementing a priority queue as a **heap** (C89 has
no standard one).

### Structure

A **min-heap**: a complete binary tree stored in a plain array. Invariant:
**every parent's key ≤ its children's keys**, so the root `items[0]` is always
the **highest-priority** request. Index math (0-based):

```
parent(i) = (i - 1) / 2
left(i)   = 2*i + 1
right(i)  = 2*i + 2
```

Capacity = **`number_of_coders`**: at most **one outstanding request per coder**
(a coder blocks until granted, so it is never queued twice).

### Operations (all O(log n))

- **`push`**: append at the end, then **sift-up** — while the new key is
  **smaller than its parent's key**, swap; stop when parent ≤ child.
- **`pop`** (top): take `items[0]`, move the **last** element to the root,
  shrink `size`, then **sift-down** — while the node is **larger than its
  smaller child's key**, swap with that child; stop when parent ≤ both children.
- **`peek`**: return `items[0]` without modifying the heap (O(1)).

Only these three are needed. **No arbitrary-remove**: during a round the arbiter
pops all requests into `scratch` to process them in priority order, then pushes
the ungranted ones back. A coder leaves permanently only by being granted or
when the simulation stops (the heap is discarded). No `valid` flag required.

### Policy: one heap, two keys

Both policies use "**smaller key wins**" (min-heap):

- `fifo` → `key = arrival_counter` (snapshotted at **enqueue**); smaller = arrived
  earlier.
- `edf`  → `key = last_compile_start + time_to_burnout` (snapshotted at
  **enqueue**); smaller = sooner deadline.

**Tie-break**: equal keys → smaller **coder id** wins (only relevant for `edf`;
`fifo` keys are unique via the monotonic counter).

### Use by the arbiter (reservation-aware)

`arbiter_run` is called after an enqueue, after a release, and when a cooldown
timeout expires. Under `table_mutex`:

1. Reset every `dongle.reserved_by` to `-1`.
2. Pop all requests from the heap into `scratch`; this yields them in priority
   order.
3. Walk `scratch` from highest to lowest priority:
   - grant a request if both its unique dongles are grantable and
     `reserved_by == -1`;
   - otherwise reserve each still-unreserved dongle for this coder and push the
     request back into the heap.
4. Broadcast `dongle_cond` if at least one request was granted.

A blocked high-priority request keeps its dongles reserved, so lower requests
that share them wait. A lower request whose pair is completely disjoint still
gets granted. This is the reservation-aware rule chosen in the Fairness
section; the older "stop at the first blocked root" behavior is **not** used.

## Main flow & initialization

### Startup order

1. **Check `argc == 9`**; on failure print usage/error to **stderr** and
   `return (1)`.
2. **Parse + validate** the arguments and fill `config`:
   - a per-argument helper checks the numeric string (non-empty, only digits,
     length-bounded) before `atoi`;
   - `strcmp` for the scheduler (`fifo`/`edf`);
   - a final `valid_config` enforces rules (e.g. `number_of_coders >= 1`).
3. **Allocate `sim`**, copy `config` in, allocate `dongles[]`, `coders[]`,
   `heap.items` and `scratch` (both capacity `number_of_coders`).
4. **Initialize** `table_mutex`, `log_mutex`, `stop_cond` and `dongle_cond`
   (`pthread_mutex_init`, `pthread_cond_init`).
5. Record **`start_ms`** (before any thread exists — it anchors first deadlines).
6. **Wire dongles** to coders (0-based):
   ```
   coder[i].left  = dongle[(i + N - 1) % N]
   coder[i].right = dongle[i]
   ```
7. **Init each coder**: `compiles_done = 0`, `finished = 0`,
   `last_compile_start = start_ms` (first deadline = `start_ms + time_to_burnout`).
8. **Create threads**: monitor first (arg = `&sim`), then coders in a `for` loop
   (arg = `&sim->coders[i]`, per Option B). A `created` counter tracks how many
   succeeded.
9. **Join** all created threads (monitor + coders) in a loop.
10. **Cleanup** (see below) and `return (0)`.

### Dongle ↔ coder mapping (0-based, circular)

Coders and dongles both indexed `0..N-1`. Dongle `i` sits between coders `i`
and `i+1`. So:

- `coder[i]`'s **left** dongle = `(i + N - 1) % N`,
- `coder[i]`'s **right** dongle = `i`.

No special cases; the modulo handles the wrap (coder `0`'s left = dongle `N-1`).

### Cleanup

Only after all joins: `pthread_mutex_destroy` (`table_mutex`, `log_mutex`),
`pthread_cond_destroy` (`stop_cond`, `dongle_cond`), then `free(scratch)`,
`free(heap.items)`, `free(coders)`, `free(dongles)`, `free(sim)`. Destroying a
mutex/condvar that is still locked or has waiters is undefined behavior, so
joins always come first.

### Error-path unwind

A failed `malloc` or `pthread_create` at the k-th thread must not leave threads
running: set `stop = 1` and broadcast both `stop_cond` and `dongle_cond` (under
the table mutex) so created coders exit, **join every thread that was created**,
destroy the mutexes/condvars, free all memory, and `return (1)`.

## File layout, Makefile & Norm

### File layout

Per the English subject ("Makefile at the root, `*.c` and `*.h` in the folder of
your choice"), keep the current structure:

```
codexion/
├── Makefile
├── src/          # *.c
├── include/      # *.h
└── ...           # README.md, summary.md
```

Binary name: **`codexion`**.

### Makefile

- `CC = cc`.
- Flags: **`-Wall -Wextra -Werror -pthread`** plus `-I include`. The `-pthread`
  flag is mandatory (the subject states the code must compile with
  `-Wall -Wextra -Werror -pthread`).
- Targets: `$(NAME)`, `all`, `clean`, `fclean`, `re`. (No `bonus` — codexion has
  no bonus part, and **libft is not allowed**.)
- **No relink**: standard dependency rules make `$(NAME)` depend only on the
  object files; with no changes, `make` prints "Nothing to be done for 'all'".
- `clean` removes objects, `fclean` also removes the binary, `re = fclean + all`.

### Norm constraints that apply

- Functions ≤ 25 lines, ≤ 4 parameters, ≤ 5 functions per file.
- C89-style: declarations at the top of a block.
- **No global variables** — satisfied by the `t_sim` design.
- Headers use **include guards**.
- File-internal helpers are **`static`**.
- → Plan files around the 5-function/25-line limits (heap sift-up/sift-down,
  parser helpers, monitor loop helpers, etc.).

## README requirements

`README.md` lives at the **root** of the repository and must be written in
**English**.

### Minimum required content (any project)

1. **First line — italicized**, reading exactly:
   `This project has been created as part of the 42 curriculum by orhernan.`
   (42 login: **orhernan**.)
2. **"Description"** — the project's goal and a brief overview.
3. **"Instructions"** — compilation, installation and/or execution (build with
   `make`, run `./codexion` with the 8 arguments).
4. **"Resources"** — classic references on the topic (docs/articles/tutorials on
   POSIX threads, mutexes, condition variables, priority queues, the dining
   philosophers problem, EDF scheduling) **plus** a description of how AI was
   used, specifying **for which tasks and which parts of the project**.

### Sections required specifically for codexion

1. **"Blocking cases handled"** — document the five concurrency issues:
   - deadlock prevention and Coffman's conditions,
   - starvation prevention (EDF / strict-priority reservation),
   - cooldown handling,
   - precise burnout detection (≤ 10 ms),
   - log serialization.
   (All five are already fully derived in this `summary.md` — Physical model,
   Blocking cases handled, Monitor & timing, Dongle arbitration.)
2. **"Thread synchronization mechanisms"** — explain the primitives used
   (`pthread_mutex_t`, `pthread_cond_t`, the interruptible-wait/timed-wait usage
   that acts as a custom event) and how they coordinate access to the shared
   resources (**dongles, logging, monitor state**). Include concrete examples of
   race-condition prevention and of thread-safe communication between coders and
   the monitor.

### AI-usage disclosure (to include in "Resources")

The AI was used for: guided planning and Socratic design of this solution (this
`summary.md`), explanation of POSIX threading concepts, validation/correction of
the concurrency design, and structuring of the implementation plan. The user
understands and is responsible for all of it. Actual implementation is still to
be done by the user.

## Testing & evaluation strategy

### Tools (in the `tests/` scaffolding, not the deliverable)

- **Heap + parser + log unit tests**: plain **`assert`/CHECK** tests, no external
  C framework, via a dedicated `make test` target.
- **Memory**: `valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1
  ./codexion <args>`.
- **Data races**: TSAN build (`-fsanitize=thread`) or `valgrind
  --tool=helgrind`. Debug/sanitize builds use a **separate target** — the
  deliverable compiles with exactly `-Wall -Wextra -Werror -pthread`.
- **Behaviour checker** (own script): run the binary, capture stdout, then
  validate the log rules (formats, ordering, counts, timestamps).

### Edge cases to test

- 1 coder / 1 dongle (one `has taken a dongle` per compile), cycles `n` times,
  no deadlock.
- 2 coders — the exact ring-deadlock configuration; both must compile.
- 4 coders — reservation-aware check: a lower-priority request with a fully
  disjoint pair must compile while a higher-priority request is blocked; a lower
  request sharing a reserved dongle must wait.
- Complete-all path: each coder prints exactly `n` `is compiling` lines, no
  `burned out`, exit code 0.
- Burnout path: exactly **one** `burned out` line, nothing logged after it.
- Cooldown extremes: `0` vs large; contention with staggered coders.
- `number_of_compiles_required = 1` smoke test; `0` exits cleanly.
- EDF (feasible) ⇒ no burnout; FIFO ⇒ grants follow arrival order.
- Zero time values; large `number_of_coders` (50/100) stress.
- Invalid-input battery: negative, `"12abc"`, `""`, overflow-length, bad
  scheduler, `0` coders ⇒ usage on stderr + exit 1.

### Burnout-precision check (< 10 ms)

For the burned-out coder `X`:
`burnout_ts - (X's last "is compiling" ts + time_to_burnout) ≤ 10 ms`
(tolerating OS scheduling jitter per the subject). Also verify every coder's
gap between consecutive compile starts is `≤ time_to_burnout` (no missed
deadline went undetected).

### Demo parameters

- **Clean (EDF)**: `5 800 200 200 200 3 50 edf` — runs to completion, no burnout.
- **Burnout**: `4 300 150 300 300 10 100 fifo` — tight deadline + long phases +
  cooldown ⇒ one burnout within 10 ms.
- Re-verify feasibility before the defense; show both paths.

### Defense / "recode" readiness

- Small single-purpose functions; separation between **coder logic, arbiter,
  monitor, logging** so a small behavior tweak is localized to one function.
- Anything an evaluator might change is a **data/config value**, not a constant.
- Following the Norm gives the structure the recode step wants.
