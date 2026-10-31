#include "test.h"
#include "codexion.h"

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

void	test_init(void)
{
	t_config	config;
	t_table		*table;
	int			i;

	config = make_config(1);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	CHECK(table->config.number_of_coders == 1);
	CHECK(table->config.time_to_burnout == 800);
	CHECK(table->config.policy == EDF);
	CHECK(table->dongles != NULL);
	CHECK(table->coders != NULL);
	CHECK(table->heap.items != NULL);
	CHECK(table->scratch != NULL);
	CHECK(table->heap.size == 0);
	CHECK(table->stop == false);
	CHECK(table->finished_counter == 0);
	CHECK(table->arrival_counter == 0);
	CHECK(table->dongles[0].id == 0);
	CHECK(table->dongles[0].available_at == table->start_ms);
	CHECK(table->dongles[0].holder == -1);
	CHECK(table->dongles[0].reserved_by == -1);
	CHECK(table->coders[0].table == table);
	CHECK(table->coders[0].id == 1);
	CHECK(table->coders[0].last_compile_start == table->start_ms);
	CHECK(table->coders[0].compiles_done == 0);
	CHECK(table->coders[0].finished == false);
	CHECK(table->coders[0].left == &table->dongles[0]);
	CHECK(table->coders[0].right == &table->dongles[0]);
	cleanup_table(table);
	config = make_config(4);
	table = init_table(&config);
	CHECK(table != NULL);
	if (!table)
		return ;
	i = 0;
	while (i < 4)
	{
		CHECK(table->dongles[i].id == i);
		CHECK(table->dongles[i].available_at == table->start_ms);
		CHECK(table->dongles[i].holder == -1);
		CHECK(table->dongles[i].reserved_by == -1);
		CHECK(table->coders[i].table == table);
		CHECK(table->coders[i].id == i + 1);
		CHECK(table->coders[i].last_compile_start == table->start_ms);
		CHECK(table->coders[i].compiles_done == 0);
		CHECK(table->coders[i].finished == false);
		CHECK(table->coders[i].left == &table->dongles[(i + 3) % 4]);
		CHECK(table->coders[i].right == &table->dongles[i]);
		i++;
	}
	cleanup_table(table);
	cleanup_table(NULL);
}
