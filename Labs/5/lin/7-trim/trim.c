/**
  * SO, 2016
  * Lab #5
  *
  * Task #7, lin
  *
  * Use of mcheck
  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

char first_name[] = "  Harry";
char last_name[]  = "    Potter";

/*
static char *trim(char *s)
{

	char *p = malloc(strlen(s)+1);

	strcpy(p, s);

	while (*p == ' ')
		p++;

	strcpy(s, p);
	free(p);

	return s;
}
*/

static char *trim(char *s)
{

	char *p = malloc(strlen(s)+1);

	strcpy(p, s);
	
	char *q = p;

	while (*q == ' ')
		q++;

	strcpy(s, q);
	free(p);

	return s;
}

int main(void)
{

	printf("%s %s is learning SO!\n",
			trim(first_name), trim(last_name));

	return 0;
}
