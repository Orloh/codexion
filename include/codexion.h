/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   codexion.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: orhernan <orhernan@student.42madrid.com>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/12 14:04:16 by orhernan          #+#    #+#             */
/*   Updated: 2026/11/01 20:52:08 by orhernan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CODEXION_H
# define CODEXION_H

# include <pthread.h>

/* Data structs */

typedef struct s_table	t_table;

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
	int				id;
	long			available_at;
	int				holder;
	pthread_mutex_t	mutex;
}	t_dongle;

typedef struct s_coder
{
	t_table		*table;
	int			id;
	long		last_compile_start;
	int			compiles_done;
	int			finished;
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

typedef struct s_shared
{
	t_heap			heap;
	t_request		*scratch;
	int				stop;
	int				finished_counter;
	long			start_ms;
	long			arrival_counter;
	pthread_mutex_t	mutex;
	pthread_cond_t	stop_cond;
	pthread_cond_t	dongle_cond;
}	t_shared;

typedef struct s_table
{
	t_config		config;
	t_shared		shared;
	t_dongle		*dongles;
	t_coder			*coders;
	pthread_mutex_t	log_mutex;
	int				dongles_ready;
}	t_table;

/* Functions */

void		heap_push(t_heap *heap, t_request req);
t_request	heap_pop(t_heap *heap);
t_request	*heap_peek(t_heap *heap);
int			parse_args(int argc, char **argv, t_config *config);
long		get_now_ms(void);
t_table		*init_table(t_config *config);
void		cleanup_table(t_table *table);
void		arbiter_request(t_coder *coder);
void		arbiter_release(t_coder *coder);
void		arbiter_run(t_table *table, long now);
void		arbiter_lock_pair(t_coder *coder);
void		arbiter_unlock_pair(t_coder *coder);
int			arbiter_try_pair(t_coder *coder, long now);
int			sleep_phase(t_table *table, long duration);
int			stop_requested(t_table *table);
int			log_format(char *buf, long timestamp, int coder_id,
				const char *state);
void		log_state(t_table *table, int coder_id, const char *state);
void		*coder_routine(void *coder_ptr);
void		*monitor_routine(void *table_ptr);

#endif
