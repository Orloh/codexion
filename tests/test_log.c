#include "test.h"
#include "codexion.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static void	check_format(long timestamp, int coder_id, const char *state,
		const char *expected)
{
	char	buf[128];

	CHECK(log_format(buf, timestamp, coder_id, state) == (int)strlen(expected));
	CHECK(strcmp(buf, expected) == 0);
}

static long	now_ms(void)
{
	struct timeval	tv;

	(void)gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

static int	capture_log(t_sim *sim, int coder_id, const char *state, char *buf)
{
	int	fds[2];
	int	saved;
	int	n;

	if (pipe(fds) != 0)
		return (-1);
	saved = dup(1);
	if (saved < 0)
		return (-1);
	fflush(stdout);
	(void)dup2(fds[1], 1);
	close(fds[1]);
	log_state(sim, coder_id, state);
	(void)dup2(saved, 1);
	close(saved);
	n = (int)read(fds[0], buf, 255);
	close(fds[0]);
	if (n < 0)
		return (-1);
	buf[n] = '\0';
	return (n);
}

void	test_log(void)
{
	check_format(0, 1, "has taken a dongle", "0 1 has taken a dongle\n");
	check_format(12, 3, "is compiling", "12 3 is compiling\n");
	check_format(1204, 3, "burned out", "1204 3 burned out\n");
	check_format(0, 0, "is debugging", "0 0 is debugging\n");
	check_format(1, 2, "", "1 2 \n");
	check_format(1, 2, "is refactoring", "1 2 is refactoring\n");
#if LONG_MAX > INT_MAX
	check_format(2147483648L, 42, "is refactoring",
		"2147483648 42 is refactoring\n");
#endif
}

void	test_log_state(void)
{
	t_sim	sim;
	char	buf[256];
	char	*end;
	long	ts;
	int		n;

	memset(&sim, 0, sizeof(sim));
	sim.start_ms = now_ms() - 100;
	if (pthread_mutex_init(&sim.log_mutex, NULL) != 0)
	{
		CHECK(0);
		return ;
	}
	n = capture_log(&sim, 7, "is compiling", buf);
	CHECK(pthread_mutex_destroy(&sim.log_mutex) == 0);
	CHECK(n > 0);
	if (n <= 0)
		return ;
	CHECK(n == (int)strlen(buf));
	ts = strtol(buf, &end, 10);
	CHECK(end != buf);
	CHECK(*end == ' ');
	CHECK(strcmp(end, " 7 is compiling\n") == 0);
	CHECK(ts >= 90);
	CHECK(ts < 5000);
}
