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
#include <sys/time.h>

static int	alloc_arrays(t_sim *sim)
{
	int	n;

	n = sim->config.number_of_coders;
	sim->dongles = malloc(n * sizeof(*sim->dongles));
	sim->coders = malloc(n * sizeof(*sim->coders));
	sim->heap.items = malloc(n * sizeof(*sim->heap.items));
	sim->scratch = malloc(n * sizeof(*sim->scratch));
	if (!sim->dongles || !sim->coders
		|| !sim->heap.items || !sim->scratch)
		return (1);
	return (0);
}

static int	init_sync(t_sim *sim)
{
	if (pthread_mutex_init(&sim->table_mutex, NULL) != 0)
		return (1);
	if (pthread_mutex_init(&sim->log_mutex, NULL) != 0)
	{
		pthread_mutex_destroy(&sim->table_mutex);
		return (1);
	}
	if (pthread_cond_init(&sim->stop_cond, NULL) != 0)
	{
		pthread_mutex_destroy(&sim->log_mutex);
		pthread_mutex_destroy(&sim->table_mutex);
		return (1);
	}
	if (pthread_cond_init(&sim->dongle_cond, NULL) != 0)
	{
		pthread_cond_destroy(&sim->stop_cond);
		pthread_mutex_destroy(&sim->log_mutex);
		pthread_mutex_destroy(&sim->table_mutex);
		return (1);
	}
	return (0);
}

static void	free_sim(t_sim *sim)
{
	free(sim->scratch);
	free(sim->heap.items);
	free(sim->coders);
	free(sim->dongles);
	free(sim);
}

static void	init_table(t_sim *sim)
{
	int	i;
	int	n;

	n = sim->config.number_of_coders;
	i = 0;
	while (i < n)
	{
		sim->dongles[i].id = i;
		sim->dongles[i].available_at = sim->start_ms;
		sim->dongles[i].holder = -1;
		sim->dongles[i].reserved_by = -1;
		sim->coders[i].sim = sim;
		sim->coders[i].id = i + 1;
		sim->coders[i].last_compile_start = sim->start_ms;
		sim->coders[i].compiles_done = 0;
		sim->coders[i].finished = false;
		sim->coders[i].left = &sim->dongles[(i + n - 1) % n];
		sim->coders[i].right = &sim->dongles[i];
		i++;
	}
}

t_sim	*init_sim(t_config *config)
{
	t_sim			*sim;
	struct timeval	tv;

	sim = malloc(sizeof(*sim));
	if (!sim)
		return (NULL);
	memset(sim, 0, sizeof(*sim));
	sim->config = *config;
	if (alloc_arrays(sim) != 0)
	{
		free_sim(sim);
		return (NULL);
	}
	if (init_sync(sim) != 0)
	{
		free_sim(sim);
		return (NULL);
	}
	(void)gettimeofday(&tv, NULL);
	sim->start_ms = tv.tv_sec * 1000L + tv.tv_usec / 1000L;
	init_table(sim);
	return (sim);
}
