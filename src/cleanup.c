/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cleanup.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 00:02:24 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/26 00:02:24 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <stdlib.h>

void	cleanup_sim(t_sim *sim)
{
	if (!sim)
		return ;
	pthread_mutex_destroy(&sim->table_mutex);
	pthread_mutex_destroy(&sim->log_mutex);
	pthread_cond_destroy(&sim->stop_cond);
	pthread_cond_destroy(&sim->dongle_cond);
	free(sim->scratch);
	free(sim->heap.items);
	free(sim->coders);
	free(sim->dongles);
	free(sim);
}
