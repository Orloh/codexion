#include "test.h"
#include "codexion.h"

static t_config	make_config(int number_of_coders, long cooldown)
{
	t_config	config;

	config.number_of_coders = number_of_coders;
	config.time_to_burnout = 1000;
	config.time_to_compile = 10;
	config.time_to_debug = 10;
	config.time_to_refactor = 10;
	config.number_of_compiles_required = 1;
	config.dongle_cooldown = cooldown;
	config.policy = EDF;
	return (config);
}

static void	test_pair_lock_n1(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(1, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	arbiter_lock_pair(&table->coders[0]);
	arbiter_unlock_pair(&table->coders[0]);
	arbiter_lock_pair(&table->coders[0]);
	arbiter_unlock_pair(&table->coders[0]);
	cleanup_table(table);
}

static void	test_pair_lock_n4(void)
{
	t_config	config;
	t_table		*table;
	int			i;

	config = make_config(4, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	i = 0;
	while (i < 4)
	{
		arbiter_lock_pair(&table->coders[i]);
		arbiter_unlock_pair(&table->coders[i]);
		i++;
	}
	cleanup_table(table);
}

static void	test_try_pair_grant(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(1, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	CHECK(arbiter_try_pair(&table->coders[0], get_now_ms()) == 1);
	CHECK(table->dongles[0].holder == 1);
	cleanup_table(table);
}

static void	test_try_pair_reserve(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(2, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->dongles[0].holder = 2;
	CHECK(arbiter_try_pair(&table->coders[0], get_now_ms()) == 0);
	CHECK(table->dongles[0].holder == 2);
	CHECK(table->dongles[0].reserved_by == 1);
	CHECK(table->dongles[1].reserved_by == 1);
	cleanup_table(table);
}

static void	test_release_pair(void)
{
	t_config	config;
	t_table		*table;
	long		before;

	config = make_config(1, 100);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->dongles[0].holder = 1;
	before = get_now_ms();
	arbiter_release(&table->coders[0]);
	CHECK(table->dongles[0].holder == -1);
	CHECK(table->dongles[0].available_at >= before + 100);
	cleanup_table(table);
}

void	test_dongle_pair(void)
{
	test_pair_lock_n1();
	test_pair_lock_n4();
	test_try_pair_grant();
	test_try_pair_reserve();
	test_release_pair();
}
