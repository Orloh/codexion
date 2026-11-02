/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cleanup.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 00:02:24 by orhernan          #+#    #+#             */
/*   Updated: 2026/11/01 21:02:47 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <stdlib.h>

void	cleanup_table(t_table *table)
{
	int	i;

	if (!table)
		return ;
	pthread_mutex_destroy(&table->shared.mutex);
	pthread_mutex_destroy(&table->log_mutex);
	pthread_cond_destroy(&table->shared.stop_cond);
	pthread_cond_destroy(&table->shared.dongle_cond);
	i = 0;
	while (i < table->dongles_ready)
	{
		pthread_mutex_destroy(&table->dongles[i].mutex);
		i++;
	}
	free(table->shared.scratch);
	free(table->shared.heap.items);
	free(table->coders);
	free(table->dongles);
	free(table);
}
