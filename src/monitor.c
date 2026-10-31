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

	pthread_mutex_lock(&table->table_mutex);
	if (table->stop
		|| table->finished_counter == table->config.number_of_coders)
	{
		pthread_mutex_unlock(&table->table_mutex);
		return (-1);
	}
	id = find_burned(table, now);
	if (id != 0)
	{
		table->stop = true;
		pthread_cond_broadcast(&table->stop_cond);
		pthread_cond_broadcast(&table->dongle_cond);
	}
	pthread_mutex_unlock(&table->table_mutex);
	return (id);
}

void	*monitor_routine(void *table_ptr)
{
	t_table	*table;
	int		id;

	table = table_ptr;
	while (true)
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
