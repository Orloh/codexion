/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   arbiter.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 00:08:02 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/31 14:50:02 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	grantable(t_coder *coder, long now)
{
	if (coder->left->holder != -1 || coder->left->available_at > now
		|| coder->left->reserved_by != -1)
		return (0);
	if (coder->left == coder->right)
		return (1);
	if (coder->right->holder != -1 || coder->right->available_at > now
		|| coder->right->reserved_by != -1)
		return (0);
	return (1);
}

static int	set_pair(t_coder *coder, int grant)
{
	if (grant)
	{
		coder->left->holder = coder->id;
		if (coder->left != coder->right)
			coder->right->holder = coder->id;
	}
	else
	{
		if (coder->left->reserved_by == -1)
			coder->left->reserved_by = coder->id;
		if (coder->left != coder->right
			&& coder->right->reserved_by == -1)
			coder->right->reserved_by = coder->id;
	}
	return (grant);
}

void	arbiter_run(t_table *table, long now)
{
	t_request	req;
	int			i;
	int			granted;

	granted = 0;
	i = table->config.number_of_coders;
	while (i > 0)
		table->dongles[--i].reserved_by = -1;
	while (table->shared.heap.size > 0)
	{
		req = heap_pop(&table->shared.heap);
		if (set_pair(req.coder, grantable(req.coder, now)))
			granted = 1;
		else
			table->shared.scratch[i++] = req;
	}
	while (i > 0)
		heap_push(&table->shared.heap, table->shared.scratch[--i]);
	if (granted)
		pthread_cond_broadcast(&table->shared.dongle_cond);
}

void	arbiter_request(t_coder *coder)
{
	t_request	req;

	req.coder = coder;
	if (coder->table->config.policy == FIFO)
		req.key = coder->table->shared.arrival_counter++;
	else
		req.key = coder->last_compile_start
			+ coder->table->config.time_to_burnout;
	heap_push(&coder->table->shared.heap, req);
	arbiter_run(coder->table, get_now_ms());
}

void	arbiter_release(t_coder *coder)
{
	long	now;

	now = get_now_ms();
	coder->left->holder = -1;
	coder->left->available_at = now + coder->table->config.dongle_cooldown;
	if (coder->left != coder->right)
	{
		coder->right->holder = -1;
		coder->right->available_at = now
			+ coder->table->config.dongle_cooldown;
	}
	arbiter_run(coder->table, now);
	pthread_cond_broadcast(&coder->table->shared.dongle_cond);
}
