/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dongle_pair.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/01 23:22:47 by orhernan          #+#    #+#             */
/*   Updated: 2026/11/01 23:22:47 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

void	arbiter_lock_pair(t_coder *coder)
{
	if (coder->left->id <= coder->right->id)
	{
		pthread_mutex_lock(&coder->left->mutex);
		if (coder->left != coder->right)
			pthread_mutex_lock(&coder->right->mutex);
	}
	else
	{
		pthread_mutex_lock(&coder->right->mutex);
		pthread_mutex_lock(&coder->left->mutex);
	}
}

void	arbiter_unlock_pair(t_coder *coder)
{
	if (coder->left->id <= coder->right->id)
	{
		if (coder->left != coder->right)
			pthread_mutex_unlock(&coder->right->mutex);
		pthread_mutex_unlock(&coder->left->mutex);
	}
	else
	{
		pthread_mutex_unlock(&coder->left->mutex);
		pthread_mutex_unlock(&coder->right->mutex);
	}
}

static int	grantable(t_coder *coder, long now)
{
	if (coder->left->holder != -1 || coder->left->available_at > now
		|| coder->left->reserved_by != -1)
		return (0);
	if (coder->left == coder->right)
		return (1);
	if (coder->right->holder != -1 || coder->right->available_at > now
		|| coder->right->reserved_by != -1)
		return (0);
	return (1);
}

static void	set_pair(t_coder *coder, int grant)
{
	if (grant)
	{
		coder->left->holder = coder->id;
		if (coder->left != coder->right)
			coder->right->holder = coder->id;
	}
	else
	{
		if (coder->left->reserved_by == -1)
			coder->left->reserved_by = coder->id;
		if (coder->left != coder->right
			&& coder->right->reserved_by == -1)
			coder->right->reserved_by = coder->id;
	}
}

int	arbiter_try_pair(t_coder *coder, long now)
{
	int	grant;

	arbiter_lock_pair(coder);
	grant = grantable(coder, now);
	set_pair(coder, grant);
	arbiter_unlock_pair(coder);
	return (grant);
}
