/**
  * SO, 2016
  * Lab #5
  *
  * Task #10, lin
  *
  * Dump malloc implementation that only allocates space
  */
#include <unistd.h>
#include <sys/types.h>

#include "my_malloc.h"


void *my_malloc(size_t size)
{
	void *current = NULL;

	/* TODO - use sbrk to alloc at least size bytes */
	current = sbrk(size);
	if (current == (void*)-1)
	{
		return NULL;
	}

	/* return the start of the new allocated area */
	return current;
}
