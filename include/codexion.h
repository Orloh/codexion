/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   codexion.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/12 14:04:16 by orhernan          #+#    #+#             */
/*   Updated: 2026/10/26 00:08:02 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CODEXION_H
# define CODEXION_H

# include <pthread.h>
# include <stdbool.h>

/* Data structs */

typedef struct s_sim	t_sim;

typedef enum e_policy
{
	FIFO,
	EDF
}	t_policy;

typedef struct s_config
{
	int			number_of_coders;
	long		time_to_burnout;
	long		time_to_compile;
	long		time_to_debug;
	long		time_to_refactor;
	int			number_of_compiles_required;
	long		dongle_cooldown;
	t_policy	policy;
}	t_config;

typedef struct s_dongle
{
	int		id;
	long	available_at;
	int		holder;
	int		reserved_by;
}	t_dongle;

typedef struct s_coder
{
	t_sim		*sim;
	int			id;
	long		last_compile_start;
	int			compiles_done;
	bool		finished;
	t_dongle	*left;
	t_dongle	*right;
}	t_coder;

typedef struct s_request
{
	t_coder	*coder;
	long	key;
}	t_request;

typedef struct s_heap
{
	t_request	*items;
	int			size;
}	t_heap;

typedef struct s_sim
{
	t_config		config;
	t_dongle		*dongles;
	t_coder			*coders;
	t_heap			heap;
	t_request		*scratch;
	pthread_mutex_t	table_mutex;
	pthread_mutex_t	log_mutex;
	bool			stop;
	pthread_cond_t	stop_cond;
	pthread_cond_t	dongle_cond;
	int				finished_counter;
	long			start_ms;
	long			arrival_counter;
}	t_sim;

/* Functions */

void		heap_push(t_heap *heap, t_request req);
t_request	heap_pop(t_heap *heap);
t_request	*heap_peek(t_heap *heap);
int			parse_args(int argc, char **argv, t_config *config);
t_sim		*init_sim(t_config *config);
void		cleanup_sim(t_sim *sim);
void		arbiter_request(t_coder *coder);
void		arbiter_release(t_coder *coder);
void		arbiter_run(t_sim *sim, long now);
int			log_format(char *buf, long timestamp, int coder_id,
				const char *state);
void		log_state(t_sim *sim, int coder_id, const char *state);

#endif
