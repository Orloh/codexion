/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   time.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/31 15:38:23 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/31 15:38:23 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <errno.h>
#include <sys/time.h>
#include <time.h>

long	get_now_ms(void)
{
	struct timeval	tv;

	(void)gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

int	sleep_phase(t_table *table, long duration)
{
	struct timespec	ts;
	long			deadline;
	int				stopped;

	deadline = get_now_ms() + duration;
	ts.tv_sec = deadline / 1000;
	ts.tv_nsec = (deadline % 1000) * 1000000L;
	pthread_mutex_lock(&table->shared.mutex);
	while (!table->shared.stop)
	{
		if (pthread_cond_timedwait(&table->shared.stop_cond,
				&table->shared.mutex, &ts) == ETIMEDOUT)
			break ;
	}
	stopped = table->shared.stop;
	pthread_mutex_unlock(&table->shared.mutex);
	return (stopped);
}

int	stop_requested(t_table *table)
{
	int	stopped;

	pthread_mutex_lock(&table->shared.mutex);
	stopped = table->shared.stop;
	pthread_mutex_unlock(&table->shared.mutex);
	return (stopped);
}
