#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *show_alloc_mem_ex(void *ptr, size_t offset, size_t length);
void *show_alloc_mem();

int main()
{
	malloc(1024);
	malloc(1024 * 32);
	malloc(1024 * 1024);
	malloc(1024 * 1024 * 16);
	malloc(1024 * 1024 * 128);
	show_alloc_mem();
	write (2, "\n\n", 2);
	char *ptr = malloc(30);
	strcpy(ptr, "Hello World!\n");
	show_alloc_mem_ex(ptr, 0, 0);
	return (0);
}
