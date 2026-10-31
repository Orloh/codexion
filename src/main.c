/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/31 17:50:33 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/31 17:50:33 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <stdlib.h>

static int	start_threads(t_table *table, pthread_t *threads)
{
	int	i;
	int	created;

	if (pthread_create(&threads[0], NULL, monitor_routine, table) != 0)
		return (0);
	created = 1;
	i = 0;
	while (i < table->config.number_of_coders)
	{
		if (pthread_create(&threads[created], NULL, coder_routine,
				&table->coders[i]) != 0)
			return (created);
		created++;
		i++;
	}
	return (created);
}

static void	join_threads(pthread_t *threads, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		pthread_join(threads[i], NULL);
		i++;
	}
}

static void	stop_simulation(t_table *table)
{
	pthread_mutex_lock(&table->table_mutex);
	table->stop = true;
	pthread_cond_broadcast(&table->stop_cond);
	pthread_cond_broadcast(&table->dongle_cond);
	pthread_mutex_unlock(&table->table_mutex);
}

static int	run_simulation(t_table *table)
{
	pthread_t	*threads;
	int			created;
	int			total;

	total = table->config.number_of_coders + 1;
	threads = malloc(total * sizeof(*threads));
	if (!threads)
		return (1);
	created = start_threads(table, threads);
	if (created != total)
	{
		stop_simulation(table);
		join_threads(threads, created);
		free(threads);
		return (1);
	}
	join_threads(threads, created);
	free(threads);
	return (0);
}

int	main(int argc, char **argv)
{
	t_config	config;
	t_table		*table;
	int			status;

	if (parse_args(argc, argv, &config) != 0)
		return (1);
	table = init_table(&config);
	if (!table)
		return (1);
	status = run_simulation(table);
	cleanup_table(table);
	return (status);
}
