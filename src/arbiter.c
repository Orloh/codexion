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

void	arbiter_run(t_table *table, long now)
{
	t_request	req;
	int			i;
	int			granted;

	granted = 0;
	i = 0;
	while (table->shared.heap.size > 0)
	{
		req = heap_pop(&table->shared.heap);
		if (arbiter_try_pair(req.coder, now))
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
	arbiter_lock_pair(coder);
	coder->left->holder = -1;
	coder->left->available_at = now + coder->table->config.dongle_cooldown;
	if (coder->left != coder->right)
	{
		coder->right->holder = -1;
		coder->right->available_at = now
			+ coder->table->config.dongle_cooldown;
	}
	arbiter_unlock_pair(coder);
	pthread_mutex_lock(&coder->table->shared.mutex);
	arbiter_run(coder->table, now);
	pthread_cond_broadcast(&coder->table->shared.dongle_cond);
	pthread_mutex_unlock(&coder->table->shared.mutex);
}
