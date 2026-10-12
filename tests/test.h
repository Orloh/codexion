#ifndef TEST_H
#define TEST_H

#include <stdio.h>

extern int g_total;
extern int g_failed;

#define CHECK(cond)                                                        \
	do                                                                     \
	{                                                                      \
		g_total++;                                                         \
		if (!(cond))                                                       \
		{                                                                  \
			g_failed++;                                                    \
			(void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
		}                                                                  \
	} while (0)

#endif