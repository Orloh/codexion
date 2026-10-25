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
	t_sim		*sim;
	int			i;

	config = make_config(1);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	CHECK(sim->config.number_of_coders == 1);
	CHECK(sim->config.time_to_burnout == 800);
	CHECK(sim->config.policy == EDF);
	CHECK(sim->dongles != NULL);
	CHECK(sim->coders != NULL);
	CHECK(sim->heap.items != NULL);
	CHECK(sim->scratch != NULL);
	CHECK(sim->heap.size == 0);
	CHECK(sim->stop == 0);
	CHECK(sim->finished_counter == 0);
	CHECK(sim->arrival_counter == 0);
	CHECK(sim->dongles[0].id == 0);
	CHECK(sim->dongles[0].available_at == sim->start_ms);
	CHECK(sim->dongles[0].holder == -1);
	CHECK(sim->dongles[0].reserved_by == -1);
	CHECK(sim->coders[0].sim == sim);
	CHECK(sim->coders[0].id == 1);
	CHECK(sim->coders[0].last_compile_start == sim->start_ms);
	CHECK(sim->coders[0].compiles_done == 0);
	CHECK(sim->coders[0].finished == 0);
	CHECK(sim->coders[0].left == &sim->dongles[0]);
	CHECK(sim->coders[0].right == &sim->dongles[0]);
	cleanup_sim(sim);
	config = make_config(4);
	sim = init_sim(&config);
	CHECK(sim != NULL);
	if (!sim)
		return ;
	i = 0;
	while (i < 4)
	{
		CHECK(sim->dongles[i].id == i);
		CHECK(sim->dongles[i].available_at == sim->start_ms);
		CHECK(sim->dongles[i].holder == -1);
		CHECK(sim->dongles[i].reserved_by == -1);
		CHECK(sim->coders[i].sim == sim);
		CHECK(sim->coders[i].id == i + 1);
		CHECK(sim->coders[i].last_compile_start == sim->start_ms);
		CHECK(sim->coders[i].compiles_done == 0);
		CHECK(sim->coders[i].finished == 0);
		CHECK(sim->coders[i].left == &sim->dongles[(i + 3) % 4]);
		CHECK(sim->coders[i].right == &sim->dongles[i]);
		i++;
	}
	cleanup_sim(sim);
	cleanup_sim(NULL);
}
