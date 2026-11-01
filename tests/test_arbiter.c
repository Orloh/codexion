#include "test.h"
#include "codexion.h"
#include <limits.h>

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

static t_request	make_request(t_coder *coder, long key)
{
	t_request	req;

	req.coder = coder;
	req.key = key;
	return (req);
}

static void	reset_dongles(t_table *table, long now)
{
	int	i;

	i = 0;
	while (i < table->config.number_of_coders)
	{
		table->dongles[i].holder = -1;
		table->dongles[i].reserved_by = -1;
		table->dongles[i].available_at = now;
		i++;
	}
}

static void	test_arbiter_policy_keys(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(1);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->config.policy = FIFO;
	table->shared.arrival_counter = 7;
	table->dongles[0].available_at = LONG_MAX;
	arbiter_request(&table->coders[0]);
	CHECK(table->shared.heap.size == 1);
	CHECK(table->shared.heap.items[0].coder == &table->coders[0]);
	CHECK(table->shared.heap.items[0].key == 7);
	CHECK(table->shared.arrival_counter == 8);
	cleanup_table(table);
	config = make_config(1);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->config.policy = EDF;
	table->coders[0].last_compile_start = 1000;
	table->shared.arrival_counter = 7;
	table->dongles[0].available_at = LONG_MAX;
	arbiter_request(&table->coders[0]);
	CHECK(table->shared.heap.size == 1);
	CHECK(table->shared.heap.items[0].key == 1800);
	CHECK(table->shared.arrival_counter == 7);
	cleanup_table(table);
}

static void	test_arbiter_grant_and_release(void)
{
	t_config	config;
	t_table		*table;
	long		before;

	config = make_config(1);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	arbiter_request(&table->coders[0]);
	CHECK(table->shared.heap.size == 0);
	CHECK(table->dongles[0].holder == 1);
	CHECK(table->dongles[0].reserved_by == -1);
	before = get_now_ms();
	arbiter_release(&table->coders[0]);
	CHECK(table->dongles[0].holder == -1);
	CHECK(table->dongles[0].available_at >= before + 50);
	cleanup_table(table);
}

static void	test_arbiter_disjoint(void)
{
	t_config	config;
	t_table		*table;
	long		now;

	config = make_config(4);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	now = table->shared.start_ms + 10;
	reset_dongles(table, now);
	table->dongles[3].available_at = now + 100;
	heap_push(&table->shared.heap, make_request(&table->coders[0], 1));
	heap_push(&table->shared.heap, make_request(&table->coders[2], 2));
	arbiter_run(table, now);
	CHECK(table->dongles[1].holder == 3);
	CHECK(table->dongles[2].holder == 3);
	CHECK(table->dongles[0].holder == -1);
	CHECK(table->dongles[0].reserved_by == 1);
	CHECK(table->dongles[3].reserved_by == 1);
	CHECK(table->shared.heap.size == 1);
	CHECK(table->shared.heap.items[0].coder == &table->coders[0]);
	cleanup_table(table);
}

static void	test_arbiter_shared_reserved(void)
{
	t_config	config;
	t_table		*table;
	long		now;

	config = make_config(4);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	now = table->shared.start_ms + 10;
	reset_dongles(table, now);
	table->dongles[3].available_at = now + 100;
	heap_push(&table->shared.heap, make_request(&table->coders[0], 1));
	heap_push(&table->shared.heap, make_request(&table->coders[1], 2));
	arbiter_run(table, now);
	CHECK(table->dongles[0].holder == -1);
	CHECK(table->dongles[0].reserved_by == 1);
	CHECK(table->dongles[1].reserved_by == 2);
	CHECK(table->dongles[3].reserved_by == 1);
	CHECK(table->shared.heap.size == 2);
	cleanup_table(table);
}

static void	test_arbiter_cooldown(void)
{
	t_config	config;
	t_table		*table;
	long		now;

	config = make_config(1);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	now = table->shared.start_ms + 10;
	table->dongles[0].available_at = now + 100;
	heap_push(&table->shared.heap, make_request(&table->coders[0], 1));
	arbiter_run(table, now);
	CHECK(table->dongles[0].holder == -1);
	CHECK(table->dongles[0].reserved_by == 1);
	CHECK(table->shared.heap.size == 1);
	arbiter_run(table, now + 100);
	CHECK(table->dongles[0].holder == 1);
	CHECK(table->shared.heap.size == 0);
	cleanup_table(table);
}

void	test_arbiter(void)
{
	test_arbiter_policy_keys();
	test_arbiter_grant_and_release();
	test_arbiter_disjoint();
	test_arbiter_shared_reserved();
	test_arbiter_cooldown();
}
