#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *show_alloc_mem_ex(void *ptr, size_t offset, size_t length);
void *show_alloc_mem();

int main()
{
	char *ptr = malloc(30);
	strcpy(ptr, "Hello World!\n");
	show_alloc_mem_ex(ptr, 0, 0);
	return (0);
}
