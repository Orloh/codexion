/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   heap.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/13 21:43:18 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/24 22:13:00 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static bool	is_higher_priority(t_request *a, t_request *b)
{
	if (a->key != b->key)
		return (a->key < b->key);
	return (a->coder->id < b->coder->id);
}

static void	sift_down(t_heap *heap, t_request last)
{
	int	hole;
	int	child;

	hole = 0;
	while (2 * hole + 1 < heap->size)
	{
		child = 2 * hole + 1;
		if (child + 1 < heap->size
			&& is_higher_priority(&heap->items[child + 1],
				&heap->items[child]))
			child++;
		if (!is_higher_priority(&heap->items[child], &last))
			break ;
		heap->items[hole] = heap->items[child];
		hole = child;
	}
	heap->items[hole] = last;
}

void	heap_push(t_heap *heap, t_request req)
{
	int	hole;
	int	parent;

	heap->items[heap->size] = req;
	heap->size++;
	hole = heap->size - 1;
	while (hole > 0)
	{
		parent = (hole - 1) / 2;
		if (!is_higher_priority(&req, &heap->items[parent]))
			break ;
		heap->items[hole] = heap->items[parent];
		hole = parent;
	}
	heap->items[hole] = req;
}

t_request	heap_pop(t_heap *heap)
{
	t_request	root;

	if (heap->size <= 0)
	{
		root.coder = NULL;
		root.key = 0;
		return (root);
	}
	root = heap->items[0];
	heap->size--;
	sift_down(heap, heap->items[heap->size]);
	return (root);
}

t_request	*heap_peek(t_heap *heap)
{
	if (heap->size <= 0)
		return (NULL);
	return (&heap->items[0]);
}
