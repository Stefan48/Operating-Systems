/**
 * SO, 2017
 * Lab #6
 *
 * Task #5, lin
 *
 * Changing page access protection
 */
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "utils.h"

static int pageSize;
static struct sigaction old_action;
char *p;
int how[3] = { PROT_NONE, PROT_READ, PROT_WRITE };

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	char *addr;
	int rc;

	/* Calling the old signal handler by default for TODO 1 */
	/* Comment this line when solving TODO 2 */
	//old_action.sa_sigaction(signum, info, context);

	/* TODO 2 - check if the signal is SIGSEGV */
	if (signum != SIGSEGV)
	{
		old_action.sa_sigaction(signum, info, context);
		return;
	}
	

	/* TODO 2 - obtain from siginfo_t the memory location
	 * which caused the page fault
	 */
	addr = (char*)info->si_addr;

	/* TODO 2 - obtain the page which caused the page fault
	 * Hint: use the address returned by mmap
	 */
	int page = (addr - p) / pageSize;
	if (page < 0 || page > 2)
	{
		old_action.sa_sigaction(signum, info, context);
		return;
	}

	/* TODO 2 - increase protection for that page */
	if (how[page] == PROT_NONE)
	{
		how[page] = PROT_READ;
		rc = mprotect(p + page * pageSize, pageSize, how[page]);
		DIE(rc == -1, "mprotect");
		printf("-- page %d got PROT_READ --\n", page);
	}
	else if (how[page] == PROT_READ)
	{
		how[page] = PROT_READ | PROT_WRITE;
		rc = mprotect(p + page * pageSize, pageSize, how[page]);
		DIE(rc == -1, "mprotect");
		printf("-- page %d got PROT_READ | PROT_WRITE --\n", page);
	}
}

static void set_signal(void)
{
	struct sigaction action;
	int rc;

	action.sa_sigaction = segv_handler;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGSEGV);
	action.sa_flags = SA_SIGINFO;

	rc = sigaction(SIGSEGV, &action, &old_action);
	DIE(rc == -1, "sigaction");
}

static void restore_signal(void)
{
	struct sigaction action;
	int rc;

	action.sa_sigaction = old_action.sa_sigaction;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGSEGV);
	action.sa_flags = SA_SIGINFO;

	rc = sigaction(SIGSEGV, &action, NULL);
	DIE(rc == -1, "sigaction");
}

int main(void)
{
	int rc;
	char ch;

	pageSize = getpagesize();

	/* TODO 1 - Map 3 pages with the desired memory protection
	 * Use global 'p' variable to keep the address returned by mmap
	 * Use mprotect to set memory protections based on global 'how' array
	 * 'how' array keeps protection level for each page
	 */
	p = mmap(NULL, 3 * pageSize, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	DIE(p == MAP_FAILED, "mmap");
	
	rc = mprotect(p, pageSize, how[0]);
	DIE(rc == -1, "mprotect");
	rc = mprotect(p + pageSize, pageSize, how[1]);
	DIE(rc == -1, "mprotect");
	rc = mprotect(p + 2 * pageSize, pageSize, how[2]);
	DIE(rc == -1, "mprotect");

	set_signal();

	/* TODO 1 - Access these pages for read and write */
	
	// BEFORE exception handler
	// read
	/*
	ch = p[10]; // seg fault
	printf("%c\n", ch);
	ch = p[pageSize + 10]; // ok
	printf("%c\n", ch);
	ch = p[2 * pageSize + 10]; // ok
	printf("%c\n", ch);
	*/
	// write
	/*
	p[10] = '\0'; // seg fault
	p[pageSize + 10] = '\0'; // seg fault
	p[2 * pageSize + 10] = '\0'; // ok
	*/
	
	// AFTER exception handler
	// read
	ch = p[10]; // page 0 gets PROT_READ
	printf("%c\n", ch);
	ch = p[pageSize + 10]; // ok
	printf("%c\n", ch);
	ch = p[2 * pageSize + 10]; // ok
	printf("%c\n", ch);
	// write
	p[10] = '\0'; // page 0 gets PROT_READ | PROT_WRITE
	p[pageSize + 10] = '\0'; // page 1 gets PROT_READ | PROT_WRITE
	p[2 * pageSize + 10] = '\0'; // ok

	restore_signal();

	/* TODO 1 - unmap */
	rc = munmap(p, 3 * pageSize);
	DIE(rc == -1, "munmap");


	return 0;
}
