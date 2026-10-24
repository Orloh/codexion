#include "test.h"
#include "codexion.h"

/*
 * Heap contract (summary.md -> "Priority queue (heap)"):
 *   min-heap over an array; SMALLER key = HIGHER priority (root = items[0]).
 *   key snapshotted at enqueue: fifo = arrival counter,
 *   edf = last_compile_start + time_to_burnout.
 *   tie-break equal keys by coder id (smaller id wins).
 */

static t_request	make_req(t_coder *coder, long key)
{
	t_request	r;

	r.coder = coder;
	r.key = key;
	return (r);
}

static void	reset_heap(t_heap *heap, t_request *items)
{
	heap->items = items;
	heap->size = 0;
}

void	test_heap(void)
{
	t_heap		heap;
	t_request	items[8];
	t_coder		coders[4];
	t_request	r;
	int			i;

	i = 0;
	while (i < 4)
	{
		coders[i].id = i + 1;
		i++;
	}

	/* empty heap */
	reset_heap(&heap, items);
	CHECK(heap_peek(&heap) == NULL);
	CHECK(heap.size == 0);

	/* single element */
	reset_heap(&heap, items);
	heap_push(&heap, make_req(&coders[0], 7));
	CHECK(heap.size == 1);
	CHECK(heap_peek(&heap) != NULL);
	CHECK(heap_peek(&heap) && heap_peek(&heap)->key == 7);
	r = heap_pop(&heap);
	CHECK(r.key == 7);
	CHECK(heap.size == 0);
	CHECK(heap_peek(&heap) == NULL);

	/* sift-up: peek is the minimum */
	reset_heap(&heap, items);
	heap_push(&heap, make_req(&coders[0], 5));
	heap_push(&heap, make_req(&coders[1], 1));
	heap_push(&heap, make_req(&coders[2], 3));
	CHECK(heap_peek(&heap) && heap_peek(&heap)->key == 1);

	/* drain in ascending order */
	r = heap_pop(&heap);
	CHECK(r.key == 1);
	CHECK(heap_peek(&heap) && heap_peek(&heap)->key == 3);
	r = heap_pop(&heap);
	CHECK(r.key == 3);
	r = heap_pop(&heap);
	CHECK(r.key == 5);
	CHECK(heap.size == 0);

	/* larger scrambled set drains ascending */
	reset_heap(&heap, items);
	heap_push(&heap, make_req(&coders[0], 4));
	heap_push(&heap, make_req(&coders[1], 2));
	heap_push(&heap, make_req(&coders[2], 6));
	heap_push(&heap, make_req(&coders[3], 1));
	heap_push(&heap, make_req(&coders[0], 5));
	heap_push(&heap, make_req(&coders[1], 3));
	heap_push(&heap, make_req(&coders[2], 7));
	r = heap_pop(&heap);
	i = 1;
	while (heap.size > 0)
	{
		CHECK(r.key == i);
		r = heap_pop(&heap);
		i++;
	}
	CHECK(r.key == 7);

	/* tie-break: equal keys -> smaller coder id first */
	reset_heap(&heap, items);
	heap_push(&heap, make_req(&coders[1], 100));
	heap_push(&heap, make_req(&coders[0], 100));
	r = heap_pop(&heap);
	CHECK(r.coder && r.coder->id == 1);
	r = heap_pop(&heap);
	CHECK(r.coder && r.coder->id == 2);

	/* interleaved push/pop keeps the min on top */
	reset_heap(&heap, items);
	heap_push(&heap, make_req(&coders[0], 10));
	heap_push(&heap, make_req(&coders[1], 20));
	r = heap_pop(&heap);
	CHECK(r.key == 10);
	heap_push(&heap, make_req(&coders[2], 5));
	CHECK(heap_peek(&heap) && heap_peek(&heap)->key == 5);
	r = heap_pop(&heap);
	CHECK(r.key == 5);
	CHECK(heap_peek(&heap) && heap_peek(&heap)->key == 20);
}
