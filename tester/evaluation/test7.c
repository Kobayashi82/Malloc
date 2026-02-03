#include <stdlib.h>
#include "malloc.h"

#define M_LOGGING 8

int mallopt(int param, int value);
void show_alloc_history();

int main()
{
	mallopt(M_LOGGING, 1);
	void *a = malloc(32);
	void *b = malloc(128);
	void *c = calloc(4, 64);
	void *d = realloc(a, 64);
	void *k = malloc(64);
	void *k1 = realloc(k, 64);
	free(k1);
	malloc_usable_size(d);
	free(b);
	void *e = aligned_alloc(16, 256);
	void *m1 = memalign(32, 256);
	void *pm = NULL;
	posix_memalign(&pm, 32, 128);
	void *ra = reallocarray(c, 3, 128);
	void *va = valloc(512);
	void *pva = pvalloc(1024);
	void *f = realloc(d, 8);
	free(e);
	free(m1);
	free(pm);
	free(ra);
	free(va);
	free(pva);
	free(f);
	show_alloc_history();
	return (0);
}
