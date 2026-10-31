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

void	cleanup_table(t_table *table)
{
	if (!table)
		return ;
	pthread_mutex_destroy(&table->table_mutex);
	pthread_mutex_destroy(&table->log_mutex);
	pthread_cond_destroy(&table->stop_cond);
	pthread_cond_destroy(&table->dongle_cond);
	free(table->scratch);
	free(table->heap.items);
	free(table->coders);
	free(table->dongles);
	free(table);
}
