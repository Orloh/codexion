/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   coder.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/31 15:56:02 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/31 16:51:59 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <errno.h>
#include <pthread.h>
#include <time.h>

static int	sleep_phase(t_table *table, long duration)
{
	struct timespec	ts;
	long			deadline;
	int				stopped;

	deadline = get_now_ms() + duration;
	ts.tv_sec = deadline / 1000;
	ts.tv_nsec = (deadline % 1000) * 1000000L;
	pthread_mutex_lock(&table->table_mutex);
	while (!table->stop)
	{
		if (pthread_cond_timedwait(&table->stop_cond,
				&table->table_mutex, &ts) == ETIMEDOUT)
			break ;
	}
	stopped = table->stop;
	pthread_mutex_unlock(&table->table_mutex);
	return (stopped);
}

static int	wait_for_dongles(t_coder *coder)
{
	struct timespec	ts;
	long			wait_at;

	while (!coder->table->stop)
	{
		if (coder->left->holder == coder->id
			&& coder->right->holder == coder->id)
			return (0);
		wait_at = coder->left->available_at;
		if (coder->right->available_at > wait_at)
			wait_at = coder->right->available_at;
		if (wait_at < get_now_ms())
			wait_at = get_now_ms() + 1000L;
		ts.tv_sec = wait_at / 1000;
		ts.tv_nsec = (wait_at % 1000) * 1000000L;
		if (pthread_cond_timedwait(&coder->table->dongle_cond,
				&coder->table->table_mutex, &ts) == ETIMEDOUT)
			arbiter_run(coder->table, get_now_ms());
	}
	return (1);
}

static int	acquire_dongles(t_coder *coder)
{
	t_table	*table;

	table = coder->table;
	pthread_mutex_lock(&table->table_mutex);
	if (table->stop)
	{
		pthread_mutex_unlock(&table->table_mutex);
		return (1);
	}
	arbiter_request(coder);
	if (wait_for_dongles(coder))
	{
		pthread_mutex_unlock(&table->table_mutex);
		return (1);
	}
	coder->last_compile_start = get_now_ms();
	pthread_mutex_unlock(&table->table_mutex);
	log_state(table, coder->id, "has taken a dongle");
	if (coder->left != coder->right)
		log_state(table, coder->id, "has taken a dongle");
	return (0);
}

static int	run_cycle(t_coder *coder)
{
	t_table	*table;

	table = coder->table;
	if (acquire_dongles(coder))
		return (1);
	log_state(table, coder->id, "is compiling");
	if (sleep_phase(table, table->config.time_to_compile))
		return (1);
	pthread_mutex_lock(&table->table_mutex);
	coder->compiles_done++;
	arbiter_release(coder);
	pthread_mutex_unlock(&table->table_mutex);
	log_state(table, coder->id, "is debugging");
	if (sleep_phase(table, table->config.time_to_debug))
		return (1);
	log_state(table, coder->id, "is refactoring");
	return (sleep_phase(table, table->config.time_to_refactor));
}

void	*coder_routine(void *coder_ptr)
{
	t_coder	*coder;
	t_table	*table;

	coder = coder_ptr;
	table = coder->table;
	while (1)
	{
		pthread_mutex_lock(&table->table_mutex);
		if (table->stop)
		{
			pthread_mutex_unlock(&table->table_mutex);
			return (NULL);
		}
		if (coder->compiles_done >= table->config.number_of_compiles_required)
		{
			coder->finished = 1;
			table->finished_counter++;
			pthread_mutex_unlock(&table->table_mutex);
			return (NULL);
		}
		pthread_mutex_unlock(&table->table_mutex);
		if (run_cycle(coder))
			return (NULL);
	}
}
