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

void	log_state(t_table *table, int coder_id, const char *state)
{
	char			buf[128];
	int				len;
	ssize_t			ret;

	pthread_mutex_lock(&table->log_mutex);
	len = log_format(buf, get_now_ms() - table->shared.start_ms,
			coder_id, state);
	ret = write(1, buf, len);
	(void)ret;
	pthread_mutex_unlock(&table->log_mutex);
}
