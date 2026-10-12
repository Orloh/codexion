#include "test.h"

/*
 * Heap contract (summary.md -> "Priority queue (heap)"):
 *   - min-heap over an array; SMALLER key = HIGHER priority (root = items[0]).
 *   - push: append then sift-up (swap while child.key < parent.key).
 *   - pop : take root, move last element to root, shrink, sift-down.
 *   - peek: read the root without modifying the heap.
 *   - capacity = number_of_coders (at most one outstanding request per coder).
 *   - key snapshotted at enqueue: fifo = arrival counter,
 *     edf = last_compile_start + time_to_burnout.
 *   - tie-break equal keys by coder id (smaller id wins).
 *
 * TODO: implement the heap, then fill this in. Example assertions:
 *   - push keys {5, 1, 3}; CHECK(peek() key == 1)
 *   - after each pop, the next peek() key is >= the one just popped
 *   - fifo keys come out in arrival order, not coder order
 *   - two equal keys: the smaller coder id pops first
 */
void	test_heap(void)
{
}