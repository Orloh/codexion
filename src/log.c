/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   log.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/25 00:05:07 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/25 10:28:16 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static int	append_ulong(char *buf, int pos, unsigned long value)
{
	char	tmp[20];
	int		len;

	len = 0;
	if (value == 0)
		tmp[len++] = '0';
	while (value > 0)
	{
		tmp[len++] = (char)('0' + (value % 10));
		value /= 10;
	}
	while (len > 0)
		buf[pos++] = tmp[--len];
	return (pos);
}

static int	append_str(char *buf, int pos, const char *s)
{
	while (*s)
		buf[pos++] = *s++;
	return (pos);
}

int	log_format(char *buf, long timestamp, int coder_id, const char *state)
{
	int	pos;

	pos = 0;
	pos = append_ulong(buf, pos, (unsigned long)timestamp);
	buf[pos++] = ' ';
	pos = append_ulong(buf, pos, (unsigned long)coder_id);
	buf[pos++] = ' ';
	pos = append_str(buf, pos, state);
	buf[pos++] = '\n';
	buf[pos] = '\0';
	return (pos);
}

void	log_state(t_sim *sim, int coder_id, const char *state)
{
	struct timeval	tv;
	char			buf[128];
	long			now;
	int				len;

	pthread_mutex_lock(&sim->log_mutex);
	(void)gettimeofday(&tv, NULL);
	now = tv.tv_sec * 1000L + tv.tv_usec / 1000L;
	len = log_format(buf, now - sim->start_ms, coder_id, state);
	(void)write(1, buf, len);
	pthread_mutex_unlock(&sim->log_mutex);
}
