/**
 * SO, 2014
 * Lab #6, Memoria virtuala
 *
 * Task #6, Windows
 *
 * Locking memory in order to avoid swaping
 */

/* do not use UNICODE */
#undef _UNICODE
#undef UNICODE

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <windows.h>

#include "utils.h"

#define SIZE	128

static int pageSize = 0x1000;
char msg[] = "Very important real-time data";

/* lock memory interval [addr, addr + size - 1] */
static void lock_memory(char *addr, DWORD size)
{
	DWORD rc;
	DWORD page_offset = (DWORD) addr % pageSize;

	/* TODO - align addr to page offset and adjust size */
	addr -= page_offset;
	size += page_offset;

	/* TODO - lock memory */
	rc = VirtualLock(addr, size);
	DIE(rc == FALSE, "VirtualLock");
}

/* unlock memory interval [addr, addr + size - 1] */
static void unlock_memory(char *addr, DWORD size)
{
	int rc;
	DWORD page_offset = (DWORD) addr%pageSize;

	/* TODO - align addr to page offset and adjust size */
	addr -= page_offset;
	size += page_offset;

	/* TODO - unlock memory */
	rc = VirtualUnlock(addr, size);
	DIE(rc == FALSE, "VirtualUnlock");
}

int main(void)
{
	char data[SIZE];

	/* TODO - lock data */
	lock_memory(data, SIZE);

	/* TODO - copy msg into data */
	memcpy(data, msg, sizeof(msg));


	/* printing data to see it's there */
	printf("data=%s\n", data);

	/* TODO - unlock data */
	unlock_memory(data, SIZE);

	return 0;
}
