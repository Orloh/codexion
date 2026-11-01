/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   init.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/25 22:21:32 by orhernan          #+#    #+#             */
/*   Updated: 2026/11/01 20:55:27 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <stdlib.h>
#include <string.h>

static int	alloc_arrays(t_table *table)
{
	int	n;

	n = table->config.number_of_coders;
	table->dongles = malloc(n * sizeof(*table->dongles));
	table->coders = malloc(n * sizeof(*table->coders));
	table->shared.heap.items = malloc(n * sizeof(*table->shared.heap.items));
	table->shared.scratch = malloc(n * sizeof(*table->shared.scratch));
	if (!table->dongles || !table->coders
		|| !table->shared.heap.items || !table->shared.scratch)
		return (1);
	return (0);
}

static int	init_sync(t_table *table)
{
	if (pthread_mutex_init(&table->shared.mutex, NULL) != 0)
		return (1);
	if (pthread_mutex_init(&table->log_mutex, NULL) != 0)
	{
		pthread_mutex_destroy(&table->shared.mutex);
		return (1);
	}
	if (pthread_cond_init(&table->shared.stop_cond, NULL) != 0)
	{
		pthread_mutex_destroy(&table->log_mutex);
		pthread_mutex_destroy(&table->shared.mutex);
		return (1);
	}
	if (pthread_cond_init(&table->shared.dongle_cond, NULL) != 0)
	{
		pthread_cond_destroy(&table->shared.stop_cond);
		pthread_mutex_destroy(&table->log_mutex);
		pthread_mutex_destroy(&table->shared.mutex);
		return (1);
	}
	return (0);
}

static int	init_dongle_mutexes(t_table *table)
{
	int	i;

	i = 0;
	while (i < table->config.number_of_coders)
	{
		if (pthread_mutex_init(&table->dongles[i].mutex, NULL) != 0)
			return (1);
		table->dongles_ready++;
		i++;
	}
	return (0);
}

static void	init_entries(t_table *table)
{
	int	i;
	int	n;

	n = table->config.number_of_coders;
	i = 0;
	while (i < n)
	{
		table->dongles[i].id = i;
		table->dongles[i].available_at = table->shared.start_ms;
		table->dongles[i].holder = -1;
		table->dongles[i].reserved_by = -1;
		table->coders[i].table = table;
		table->coders[i].id = i + 1;
		table->coders[i].last_compile_start = table->shared.start_ms;
		table->coders[i].compiles_done = 0;
		table->coders[i].finished = 0;
		table->coders[i].left = &table->dongles[(i + n - 1) % n];
		table->coders[i].right = &table->dongles[i];
		i++;
	}
}

t_table	*init_table(t_config *config)
{
	t_table	*table;

	table = malloc(sizeof(*table));
	if (!table)
		return (NULL);
	memset(table, 0, sizeof(*table));
	table->config = *config;
	if (init_sync(table) != 0)
	{
		free(table);
		return (NULL);
	}
	if (alloc_arrays(table) != 0 || init_dongle_mutexes(table) != 0)
	{
		cleanup_table(table);
		return (NULL);
	}
	table->shared.start_ms = get_now_ms();
	init_entries(table);
	return (table);
}
