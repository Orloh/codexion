/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   init.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/25 22:21:32 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/25 23:21:37 by orhernan         ###   ########.fr       */
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
	table->heap.items = malloc(n * sizeof(*table->heap.items));
	table->scratch = malloc(n * sizeof(*table->scratch));
	if (!table->dongles || !table->coders
		|| !table->heap.items || !table->scratch)
		return (1);
	return (0);
}

static int	init_sync(t_table *table)
{
	if (pthread_mutex_init(&table->table_mutex, NULL) != 0)
		return (1);
	if (pthread_mutex_init(&table->log_mutex, NULL) != 0)
	{
		pthread_mutex_destroy(&table->table_mutex);
		return (1);
	}
	if (pthread_cond_init(&table->stop_cond, NULL) != 0)
	{
		pthread_mutex_destroy(&table->log_mutex);
		pthread_mutex_destroy(&table->table_mutex);
		return (1);
	}
	if (pthread_cond_init(&table->dongle_cond, NULL) != 0)
	{
		pthread_cond_destroy(&table->stop_cond);
		pthread_mutex_destroy(&table->log_mutex);
		pthread_mutex_destroy(&table->table_mutex);
		return (1);
	}
	return (0);
}

static void	free_table(t_table *table)
{
	free(table->scratch);
	free(table->heap.items);
	free(table->coders);
	free(table->dongles);
	free(table);
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
		table->dongles[i].available_at = table->start_ms;
		table->dongles[i].holder = -1;
		table->dongles[i].reserved_by = -1;
		table->coders[i].table = table;
		table->coders[i].id = i + 1;
		table->coders[i].last_compile_start = table->start_ms;
		table->coders[i].compiles_done = 0;
		table->coders[i].finished = false;
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
	if (alloc_arrays(table) != 0)
	{
		free_table(table);
		return (NULL);
	}
	if (init_sync(table) != 0)
	{
		free_table(table);
		return (NULL);
	}
	table->start_ms = get_now_ms();
	init_entries(table);
	return (table);
}
