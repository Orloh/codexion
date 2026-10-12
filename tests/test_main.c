#include "test.h"

int	g_total	= 0;
int	g_failed	= 0;

void	test_heap(void);
void	test_parse(void);

int	main(void)
{
	test_heap();
	test_parse();
	printf("%d/%d checks passed\n", g_total - g_failed, g_total);
	return (g_failed != 0);
}