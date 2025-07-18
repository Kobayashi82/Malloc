#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void print(char *s)
{
	write(1, s, strlen(s));
}

int main()
{
	char *addr;

	addr = malloc(16);
	free(NULL);
	free((void *)addr + 5);
	if (realloc((void *)addr + 5, 10) == NULL)
		print("Hello\n");
	if (realloc((void *)addr + 5, 0) == NULL)
		print("Hello\n");
}

// Note: Use MALLOC_CHECK_=1 or MALLOC_CHECK_=2 to prevent abort on invalid pointer