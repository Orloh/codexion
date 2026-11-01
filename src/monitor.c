/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   monitor.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/31 17:22:54 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/31 17:41:31 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define _XOPEN_SOURCE 500
#include "codexion.h"
#include <pthread.h>
#include <unistd.h>

static int	find_burned(t_table *table, long now)
{
	int		i;
	t_coder	*coder;

	i = 0;
	while (i < table->config.number_of_coders)
	{
		coder = &table->coders[i];
		if (!coder->finished
			&& coder->compiles_done
			< table->config.number_of_compiles_required
			&& now > coder->last_compile_start
			+ table->config.time_to_burnout)
			return (coder->id);
		i++;
	}
	return (0);
}

static int	check_state(t_table *table, long now)
{
	int	id;

	pthread_mutex_lock(&table->shared.mutex);
	if (table->shared.stop
		|| table->shared.finished_counter == table->config.number_of_coders)
	{
		pthread_mutex_unlock(&table->shared.mutex);
		return (-1);
	}
	id = find_burned(table, now);
	if (id != 0)
	{
		table->shared.stop = 1;
		pthread_cond_broadcast(&table->shared.stop_cond);
		pthread_cond_broadcast(&table->shared.dongle_cond);
	}
	pthread_mutex_unlock(&table->shared.mutex);
	return (id);
}

void	*monitor_routine(void *table_ptr)
{
	t_table	*table;
	int		id;

	table = table_ptr;
	while (1)
	{
		id = check_state(table, get_now_ms());
		if (id == -1)
			return (NULL);
		if (id > 0)
		{
			log_state(table, id, "burned out");
			return (NULL);
		}
		usleep(1000);
	}
}
