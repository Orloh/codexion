#include "test.h"
#include "codexion.h"
#include <fcntl.h>
#include <unistd.h>

static t_config	make_config(int number_of_coders, int required,
		long compile_ms, long cooldown)
{
	t_config	config;

	config.number_of_coders = number_of_coders;
	config.time_to_burnout = 5000;
	config.time_to_compile = compile_ms;
	config.time_to_debug = 0;
	config.time_to_refactor = 0;
	config.number_of_compiles_required = required;
	config.dongle_cooldown = cooldown;
	config.policy = EDF;
	return (config);
}

static int	silence_stdout(void)
{
	int	saved;
	int	fd;

	saved = dup(1);
	fd = open("/dev/null", O_WRONLY);
	if (fd >= 0)
	{
		(void)dup2(fd, 1);
		close(fd);
	}
	return (saved);
}

static void	restore_stdout(int saved)
{
	(void)dup2(saved, 1);
	close(saved);
}

static int	start_coders(t_table *table, pthread_t *threads)
{
	int	i;

	i = 0;
	while (i < table->config.number_of_coders)
	{
		if (pthread_create(&threads[i], NULL, coder_routine,
				&table->coders[i]) != 0)
			return (1);
		i++;
	}
	return (0);
}

static void	join_coders(t_table *table, pthread_t *threads)
{
	int	i;

	i = 0;
	while (i < table->config.number_of_coders)
	{
		pthread_join(threads[i], NULL);
		i++;
	}
}

static int	run_coders(t_table *table)
{
	pthread_t	threads[4];
	int			saved;

	saved = silence_stdout();
	if (start_coders(table, threads) != 0)
	{
		restore_stdout(saved);
		return (1);
	}
	join_coders(table, threads);
	restore_stdout(saved);
	return (0);
}

static void	test_coder_single_completion(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(1, 1, 0, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	CHECK(run_coders(table) == 0);
	CHECK(table->coders[0].finished == true);
	CHECK(table->coders[0].compiles_done == 1);
	CHECK(table->finished_counter == 1);
	cleanup_table(table);
}

static void	test_coder_required_zero(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(1, 0, 0, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	CHECK(run_coders(table) == 0);
	CHECK(table->coders[0].finished == true);
	CHECK(table->coders[0].compiles_done == 0);
	CHECK(table->finished_counter == 1);
	cleanup_table(table);
}

static void	test_coder_two_coders(void)
{
	t_config	config;
	t_table		*table;

	config = make_config(2, 1, 0, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	CHECK(run_coders(table) == 0);
	CHECK(table->coders[0].finished == true);
	CHECK(table->coders[1].finished == true);
	CHECK(table->coders[0].compiles_done == 1);
	CHECK(table->coders[1].compiles_done == 1);
	CHECK(table->finished_counter == 2);
	cleanup_table(table);
}

static void	test_coder_cooldown(void)
{
	t_config	config;
	t_table		*table;
	long		before;
	long		elapsed;

	config = make_config(1, 2, 0, 50);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	before = get_now_ms();
	CHECK(run_coders(table) == 0);
	elapsed = get_now_ms() - before;
	CHECK(table->coders[0].finished == true);
	CHECK(table->coders[0].compiles_done == 2);
	CHECK(table->finished_counter == 1);
	CHECK(elapsed >= 50);
	cleanup_table(table);
}

static void	test_coder_stop_before_start(void)
{
	t_config	config;
	t_table		*table;
	pthread_t	threads[4];
	int			saved;

	config = make_config(1, 1, 0, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	pthread_mutex_lock(&table->table_mutex);
	table->stop = true;
	pthread_mutex_unlock(&table->table_mutex);
	saved = silence_stdout();
	if (start_coders(table, threads) == 0)
		join_coders(table, threads);
	restore_stdout(saved);
	CHECK(table->coders[0].finished == false);
	CHECK(table->coders[0].compiles_done == 0);
	CHECK(table->finished_counter == 0);
	cleanup_table(table);
}

static void	test_coder_stop_during_compile(void)
{
	t_config	config;
	t_table		*table;
	pthread_t	threads[4];
	int			saved;
	int			seen;
	int			i;

	config = make_config(1, 1, 1000, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	saved = silence_stdout();
	if (start_coders(table, threads) != 0)
	{
		restore_stdout(saved);
		CHECK(0);
		cleanup_table(table);
		return ;
	}
	seen = 0;
	i = 0;
	while (i < 500 && !seen)
	{
		pthread_mutex_lock(&table->table_mutex);
		seen = (table->dongles[0].holder == 1);
		pthread_mutex_unlock(&table->table_mutex);
		usleep(1000);
		i++;
	}
	pthread_mutex_lock(&table->table_mutex);
	table->stop = true;
	pthread_cond_broadcast(&table->stop_cond);
	pthread_cond_broadcast(&table->dongle_cond);
	pthread_mutex_unlock(&table->table_mutex);
	join_coders(table, threads);
	restore_stdout(saved);
	CHECK(seen == 1);
	CHECK(table->coders[0].finished == false);
	CHECK(table->coders[0].compiles_done == 0);
	CHECK(table->finished_counter == 0);
	cleanup_table(table);
}

void	test_coder(void)
{
	test_coder_single_completion();
	test_coder_required_zero();
	test_coder_two_coders();
	test_coder_cooldown();
	test_coder_stop_before_start();
	test_coder_stop_during_compile();
}
