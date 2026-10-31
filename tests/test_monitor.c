#include "test.h"
#include "codexion.h"
#include <string.h>
#include <unistd.h>

static t_config	make_config(int number_of_coders, long time_to_burnout)
{
	t_config	config;

	config.number_of_coders = number_of_coders;
	config.time_to_burnout = time_to_burnout;
	config.time_to_compile = 0;
	config.time_to_debug = 0;
	config.time_to_refactor = 0;
	config.number_of_compiles_required = 1;
	config.dongle_cooldown = 0;
	config.policy = EDF;
	return (config);
}

static int	capture_monitor(t_table *table, char *buf, int size,
		int stop_after_us)
{
	int			fds[2];
	int			saved;
	int			n;
	pthread_t	thread;

	if (pipe(fds) != 0)
		return (-1);
	saved = dup(1);
	if (saved < 0)
		return (-1);
	fflush(stdout);
	(void)dup2(fds[1], 1);
	close(fds[1]);
	if (pthread_create(&thread, NULL, monitor_routine, table) != 0)
	{
		(void)dup2(saved, 1);
		close(saved);
		close(fds[0]);
		return (-1);
	}
	if (stop_after_us >= 0)
	{
		usleep(stop_after_us);
		pthread_mutex_lock(&table->table_mutex);
		table->stop = true;
		pthread_cond_broadcast(&table->stop_cond);
		pthread_cond_broadcast(&table->dongle_cond);
		pthread_mutex_unlock(&table->table_mutex);
	}
	pthread_join(thread, NULL);
	(void)dup2(saved, 1);
	close(saved);
	n = (int)read(fds[0], buf, size - 1);
	close(fds[0]);
	if (n < 0)
		return (-1);
	buf[n] = '\0';
	return (n);
}

static void	test_monitor_all_finished(void)
{
	t_config	config;
	t_table		*table;
	char		buf[256];
	int			n;

	config = make_config(1, 100);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->finished_counter = 1;
	table->coders[0].finished = true;
	n = capture_monitor(table, buf, sizeof(buf), -1);
	CHECK(n == 0);
	CHECK(table->stop == false);
	cleanup_table(table);
}

static void	test_monitor_burnout(void)
{
	t_config	config;
	t_table		*table;
	char		buf[256];
	int			n;

	config = make_config(1, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->coders[0].last_compile_start = get_now_ms() - 100;
	n = capture_monitor(table, buf, sizeof(buf), -1);
	CHECK(n > 0);
	CHECK(strstr(buf, " 1 burned out\n") != NULL);
	CHECK(table->stop == true);
	cleanup_table(table);
}

static void	test_monitor_skips_finished(void)
{
	t_config	config;
	t_table		*table;
	char		buf[256];
	int			n;

	config = make_config(2, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->coders[0].finished = true;
	table->coders[0].compiles_done = 1;
	table->coders[0].last_compile_start = get_now_ms() - 100;
	table->coders[1].last_compile_start = get_now_ms() - 100;
	table->finished_counter = 1;
	n = capture_monitor(table, buf, sizeof(buf), -1);
	CHECK(n > 0);
	CHECK(strstr(buf, " 1 burned out\n") == NULL);
	CHECK(strstr(buf, " 2 burned out\n") != NULL);
	CHECK(table->stop == true);
	cleanup_table(table);
}

static void	test_monitor_skips_completed(void)
{
	t_config	config;
	t_table		*table;
	char		buf[256];
	int			n;

	config = make_config(1, 0);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->coders[0].compiles_done = 1;
	table->coders[0].last_compile_start = get_now_ms() - 100;
	n = capture_monitor(table, buf, sizeof(buf), 20000);
	CHECK(n == 0);
	CHECK(table->stop == true);
	cleanup_table(table);
}

static void	test_monitor_stop_already_set(void)
{
	t_config	config;
	t_table		*table;
	char		buf[256];
	int			n;

	config = make_config(1, 100);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	table->stop = true;
	n = capture_monitor(table, buf, sizeof(buf), -1);
	CHECK(n == 0);
	CHECK(table->stop == true);
	cleanup_table(table);
}

void	test_monitor(void)
{
	test_monitor_all_finished();
	test_monitor_burnout();
	test_monitor_skips_finished();
	test_monitor_skips_completed();
	test_monitor_stop_already_set();
}
