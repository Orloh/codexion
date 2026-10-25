#include "test.h"
#include "codexion.h"
#include <limits.h>

#define BASE_ARGS "codexion", "5", "800", "200", "200", "200", "3", "50", "edf"

static void	check_invalid(int index, char *value)
{
	char		*argv[] = {BASE_ARGS};
	t_config	config;

	argv[index] = value;
	CHECK(parse_args(9, argv, &config) != 0);
}

void	test_parse(void)
{
	char		*argv[] = {BASE_ARGS};
	char		*bad_values[] = {"", "-1", "+5", "12abc", " 7", "7 ",
					"0x10", "1.5", "99999999999999999999"};
	t_config	config;
	int			bad_count;
	int			i;
	int			j;

	(void)freopen("/dev/null", "w", stderr);
	CHECK(parse_args(9, argv, &config) == 0);
	CHECK(config.number_of_coders == 5);
	CHECK(config.time_to_burnout == 800);
	CHECK(config.time_to_compile == 200);
	CHECK(config.time_to_debug == 200);
	CHECK(config.time_to_refactor == 200);
	CHECK(config.number_of_compiles_required == 3);
	CHECK(config.dongle_cooldown == 50);
	CHECK(config.policy == EDF);
	CHECK(parse_args(0, NULL, &config) != 0);
	CHECK(parse_args(8, argv, &config) != 0);
	CHECK(parse_args(10, argv, &config) != 0);
	bad_count = (int)(sizeof(bad_values) / sizeof(bad_values[0]));
	i = 0;
	while (i < bad_count)
	{
		j = 1;
		while (j <= 7)
		{
			check_invalid(j, bad_values[i]);
			j++;
		}
		i++;
	}
	check_invalid(1, "0");
	check_invalid(1, "2147483648");
	check_invalid(6, "2147483648");
	check_invalid(8, "FIFO");
	check_invalid(8, "lifo");
	check_invalid(8, "");
	check_invalid(8, "fifo ");
	{
		char	*zero[] = {"codexion", "1", "0", "0", "0", "0", "0",
					"0", "fifo"};

		CHECK(parse_args(9, zero, &config) == 0);
		CHECK(config.number_of_coders == 1);
		CHECK(config.time_to_burnout == 0);
		CHECK(config.time_to_compile == 0);
		CHECK(config.time_to_debug == 0);
		CHECK(config.time_to_refactor == 0);
		CHECK(config.number_of_compiles_required == 0);
		CHECK(config.dongle_cooldown == 0);
		CHECK(config.policy == FIFO);
	}
	{
		char	*zeros[] = {"codexion", "007", "0800", "00200",
					"0200", "200", "03", "050", "edf"};

		CHECK(parse_args(9, zeros, &config) == 0);
		CHECK(config.number_of_coders == 7);
		CHECK(config.time_to_burnout == 800);
		CHECK(config.time_to_compile == 200);
		CHECK(config.time_to_debug == 200);
		CHECK(config.time_to_refactor == 200);
		CHECK(config.number_of_compiles_required == 3);
		CHECK(config.dongle_cooldown == 50);
	}
#if LONG_MAX > INT_MAX
	{
		char	*big[] = {"codexion", "1", "2147483648", "0", "0", "0",
					"1", "0", "edf"};

		CHECK(parse_args(9, big, &config) == 0);
		CHECK(config.time_to_burnout == 2147483648L);
	}
#endif
}
