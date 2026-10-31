/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   arbiter.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 00:08:02 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/26 00:08:02 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <sys/time.h>

static bool	grantable(t_coder *coder, long now)
{
	if (coder->left->holder != -1 || coder->left->available_at > now
		|| coder->left->reserved_by != -1)
		return (false);
	if (coder->left == coder->right)
		return (true);
	if (coder->right->holder != -1 || coder->right->available_at > now
		|| coder->right->reserved_by != -1)
		return (false);
	return (true);
}

static void	set_pair(t_coder *coder, bool grant)
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
}

void	arbiter_run(t_sim *sim, long now)
{
	t_request	req;
	int			i;

	i = 0;
	while (i < sim->config.number_of_coders)
		sim->dongles[i++].reserved_by = -1;
	i = 0;
	while (sim->heap.size > 0)
	{
		req = heap_pop(&sim->heap);
		if (grantable(req.coder, now))
			set_pair(req.coder, true);
		else
		{
			set_pair(req.coder, false);
			sim->scratch[i++] = req;
		}
	}
	while (i > 0)
		heap_push(&sim->heap, sim->scratch[--i]);
	pthread_cond_broadcast(&sim->dongle_cond);
}

void	arbiter_request(t_coder *coder)
{
	t_request		req;
	struct timeval	tv;

	req.coder = coder;
	if (coder->sim->config.policy == FIFO)
		req.key = coder->sim->arrival_counter++;
	else
		req.key = coder->last_compile_start
			+ coder->sim->config.time_to_burnout;
	heap_push(&coder->sim->heap, req);
	(void)gettimeofday(&tv, NULL);
	arbiter_run(coder->sim, tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

void	arbiter_release(t_coder *coder)
{
	struct timeval	tv;
	long			now;

	(void)gettimeofday(&tv, NULL);
	now = tv.tv_sec * 1000L + tv.tv_usec / 1000L;
	coder->left->holder = -1;
	coder->left->available_at = now + coder->sim->config.dongle_cooldown;
	if (coder->left != coder->right)
	{
		coder->right->holder = -1;
		coder->right->available_at = now
			+ coder->sim->config.dongle_cooldown;
	}
	arbiter_run(coder->sim, now);
}
