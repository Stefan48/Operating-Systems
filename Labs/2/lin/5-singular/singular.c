/**
 * SO, 2017
 * Lab #2, Operatii I/O simple
 *
 * Task #5, Linux
 *
 * Locking a file
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>	/* flock */
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>	/* errno */

#include "utils.h"

#define LOCK_FILE	"/tmp/my_lock_file"

static int fdlock = -1;


static void do_stuff(void)
{
	printf("Doing stuff...\n");
	sleep(10);
}


static void check_lock(void)
{
	int rc;

	/* TODO - open LOCK_FILE file */
	fdlock = open(LOCK_FILE, O_CREAT | O_RDONLY, 0444);
	DIE(fdlock < 0, "open lock file");


	/* TODO - lock the file using flock
	 * - flock must not block in any case !
	 *
	 * - in case of error - print a message showing
	 *   there is another instance running and exit
	 */
	 
	 //rc = flock(fdlock, LOCK_EX);
	 rc = flock(fdlock, LOCK_EX | LOCK_NB); // nonblocking
	 DIE(rc < 0, "flock - lock");

	printf("\nGot Lock\n\n");
}

static void clean_up(void)
{
	int rc;

	/* TODO - unlock file, close file and delete it */
	
	rc = flock(fdlock, LOCK_UN);
	DIE(rc < 0, "flock - unlock");
	
	rc = close(fdlock);
	DIE(rc < 0, "close file");
	
	rc = remove(LOCK_FILE);
	DIE(rc < 0, "delete file");
}


int main(void)
{
	check_lock();

	do_stuff();

	clean_up();

	return 0;
}


