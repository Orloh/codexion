/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parse.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/24 23:28:18 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/24 23:59:09 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int	fail(const char *message)
{
	(void)fprintf(stderr, "Error: %s\n", message);
	(void)fprintf(stderr, "Usage: ./codexion number_of_coders time_to_burnout "
		"time_to_compile time_to_debug time_to_refactor "
		"number_of_compiles_required dongle_cooldown fifo|edf\n");
	return (1);
}

static int	parse_long(const char *s, long *out)
{
	long	value;
	int		digit;

	if (!s || !*s)
		return (1);
	value = 0;
	while (*s)
	{
		if (*s < '0' || *s > '9')
			return (1);
		digit = *s - '0';
		if (value > (LONG_MAX - digit) / 10)
			return (1);
		value = value * 10 + digit;
		s++;
	}
	*out = value;
	return (0);
}

static int	parse_numeric_args(char **argv, long *values)
{
	int	i;

	i = 0;
	while (i < 7)
	{
		if (parse_long(argv[i + 1], &values[i]))
			return (1);
		i++;
	}
	return (0);
}

static int	parse_policy(const char *s, t_policy *policy)
{
	if (strcmp(s, "fifo") == 0)
		*policy = FIFO;
	else if (strcmp(s, "edf") == 0)
		*policy = EDF;
	else
		return (1);
	return (0);
}

int	parse_args(int argc, char **argv, t_config *config)
{
	long	values[7];

	if (argc != 9)
		return (fail("expected exactly 8 arguments"));
	if (parse_numeric_args(argv, values))
		return (fail("numeric arguments must be non-negative integers"));
	if (values[0] < 1 || values[0] > INT_MAX || values[5] > INT_MAX)
		return (fail("numeric argument out of range"));
	if (parse_policy(argv[8], &config->policy))
		return (fail("scheduler must be exactly fifo or edf"));
	config->number_of_coders = (int)values[0];
	config->time_to_burnout = values[1];
	config->time_to_compile = values[2];
	config->time_to_debug = values[3];
	config->time_to_refactor = values[4];
	config->number_of_compiles_required = (int)values[5];
	config->dongle_cooldown = values[6];
	return (0);
}
