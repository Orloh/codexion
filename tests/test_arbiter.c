#include "test.h"
#include "codexion.h"
#include <limits.h>
#include <sys/time.h>

static t_config	make_config(int number_of_coders)
{
	t_config	config;

	config.number_of_coders = number_of_coders;
	config.time_to_burnout = 800;
	config.time_to_compile = 200;
	config.time_to_debug = 200;
	config.time_to_refactor = 200;
	config.number_of_compiles_required = 3;
	config.dongle_cooldown = 50;
	config.policy = EDF;
	return (config);
}

static long	now_ms(void)
{
	struct timeval	tv;

	(void)gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

static t_request	make_request(t_coder *coder, long key)
{
	t_request	req;

	req.coder = coder;
	req.key = key;
	return (req);
}

static void	reset_dongles(t_sim *sim, long now)
{
	int	i;

	i = 0;
	while (i < sim->config.number_of_coders)
	{
		sim->dongles[i].holder = -1;
		sim->dongles[i].reserved_by = -1;
		sim->dongles[i].available_at = now;
		i++;
	}
}

static void	test_arbiter_policy_keys(void)
{
	t_config	config;
	t_sim		*sim;

	config = make_config(1);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	sim->config.policy = FIFO;
	sim->arrival_counter = 7;
	sim->dongles[0].available_at = LONG_MAX;
	arbiter_request(&sim->coders[0]);
	CHECK(sim->heap.size == 1);
	CHECK(sim->heap.items[0].coder == &sim->coders[0]);
	CHECK(sim->heap.items[0].key == 7);
	CHECK(sim->arrival_counter == 8);
	cleanup_sim(sim);
	config = make_config(1);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	sim->config.policy = EDF;
	sim->coders[0].last_compile_start = 1000;
	sim->arrival_counter = 7;
	sim->dongles[0].available_at = LONG_MAX;
	arbiter_request(&sim->coders[0]);
	CHECK(sim->heap.size == 1);
	CHECK(sim->heap.items[0].key == 1800);
	CHECK(sim->arrival_counter == 7);
	cleanup_sim(sim);
}

static void	test_arbiter_grant_and_release(void)
{
	t_config	config;
	t_sim		*sim;
	long		before;

	config = make_config(1);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	arbiter_request(&sim->coders[0]);
	CHECK(sim->heap.size == 0);
	CHECK(sim->dongles[0].holder == 1);
	CHECK(sim->dongles[0].reserved_by == -1);
	before = now_ms();
	arbiter_release(&sim->coders[0]);
	CHECK(sim->dongles[0].holder == -1);
	CHECK(sim->dongles[0].available_at >= before + 50);
	cleanup_sim(sim);
}

static void	test_arbiter_disjoint(void)
{
	t_config	config;
	t_sim		*sim;
	long		now;

	config = make_config(4);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	now = sim->start_ms + 10;
	reset_dongles(sim, now);
	sim->dongles[3].available_at = now + 100;
	heap_push(&sim->heap, make_request(&sim->coders[0], 1));
	heap_push(&sim->heap, make_request(&sim->coders[2], 2));
	arbiter_run(sim, now);
	CHECK(sim->dongles[1].holder == 3);
	CHECK(sim->dongles[2].holder == 3);
	CHECK(sim->dongles[0].holder == -1);
	CHECK(sim->dongles[0].reserved_by == 1);
	CHECK(sim->dongles[3].reserved_by == 1);
	CHECK(sim->heap.size == 1);
	CHECK(sim->heap.items[0].coder == &sim->coders[0]);
	cleanup_sim(sim);
}

static void	test_arbiter_shared_reserved(void)
{
	t_config	config;
	t_sim		*sim;
	long		now;

	config = make_config(4);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	now = sim->start_ms + 10;
	reset_dongles(sim, now);
	sim->dongles[3].available_at = now + 100;
	heap_push(&sim->heap, make_request(&sim->coders[0], 1));
	heap_push(&sim->heap, make_request(&sim->coders[1], 2));
	arbiter_run(sim, now);
	CHECK(sim->dongles[0].holder == -1);
	CHECK(sim->dongles[0].reserved_by == 1);
	CHECK(sim->dongles[1].reserved_by == 2);
	CHECK(sim->dongles[3].reserved_by == 1);
	CHECK(sim->heap.size == 2);
	cleanup_sim(sim);
}

static void	test_arbiter_cooldown(void)
{
	t_config	config;
	t_sim		*sim;
	long		now;

	config = make_config(1);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	now = sim->start_ms + 10;
	sim->dongles[0].available_at = now + 100;
	heap_push(&sim->heap, make_request(&sim->coders[0], 1));
	arbiter_run(sim, now);
	CHECK(sim->dongles[0].holder == -1);
	CHECK(sim->dongles[0].reserved_by == 1);
	CHECK(sim->heap.size == 1);
	arbiter_run(sim, now + 100);
	CHECK(sim->dongles[0].holder == 1);
	CHECK(sim->heap.size == 0);
	cleanup_sim(sim);
}

void	test_arbiter(void)
{
	test_arbiter_policy_keys();
	test_arbiter_grant_and_release();
	test_arbiter_disjoint();
	test_arbiter_shared_reserved();
	test_arbiter_cooldown();
}
