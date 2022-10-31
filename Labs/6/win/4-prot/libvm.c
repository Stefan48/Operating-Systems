/**
 * SO, 2014
 * Lab #6, Memoria virtuala
 *
 * Task #4, Windows
 *
 * Changing access right to pages
 */

/* do not use UNICODE */
#undef _UNICODE
#undef UNICODE

//#define _WIN32_WINNT 0x0500

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <windows.h>

#include "utils.h"

static LPVOID access_violation_handler;
static int pageSize = 0x1000;
static LPBYTE p;
static int how[3] = { PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE };

/*
 * SIGSEGV handler
 */
static LONG CALLBACK access_violation(PEXCEPTION_POINTERS ExceptionInfo)
{
	DWORD old, rc;
	LPBYTE addr;
	int pageNo;

	/* TODO 2 - get the memory location which caused the page fault */
	addr = (LPBYTE)ExceptionInfo->ExceptionRecord->ExceptionInformation[1];

	/* TODO 2 - get the page number which caused the page fault */
	pageNo = (addr - p) / pageSize;

	/* TODO 2 - test if page is one of our own */
	if (pageNo < 0 || pageNo > 2)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	/* TODO 2 - increase protection for that page */
	if (how[pageNo] == PAGE_NOACCESS)
	{
		how[pageNo] = PAGE_READONLY;
		rc = VirtualProtect(p + pageNo * pageSize, pageSize, PAGE_READONLY, &old);
		DIE(rc == FALSE, "VirtualProtect");
		printf("page %d got PAGE_READONLY\n", pageNo);
	}
	else if (how[pageNo] == PAGE_READONLY)
	{
		how[pageNo] = PAGE_READWRITE;
		rc = VirtualProtect(p + pageNo * pageSize, pageSize, PAGE_READWRITE, &old);
		DIE(rc == FALSE, "VirtualProtect");
		printf("page %d got PAGE_READWRITE\n", pageNo);
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

/*
 * sets SIGSEGV handler
 */
static void set_signal(void)
{
	/* TODO 2 - add VectoredHandler */
	access_violation_handler = AddVectoredExceptionHandler(1, access_violation);
	DIE(access_violation_handler == NULL, "AddVectoredExceptionHandler");
}

/*
 * restores SIGSEGV handler
 */
static void restore_signal(void)
{
	/* TODO 2 - remove VectoredHandler */
	int rc = RemoveVectoredExceptionHandler(access_violation_handler);
	DIE(rc == FALSE, "RemoveVectoredExceptionHandler");
}

int main(void)
{
	BYTE ch;
	DWORD old, rc;

	/* TODO 1 - Map 3 pages with the desired memory protection
	 * Use global 'p' variable to keep the address return by VirtualAlloc
	 * Use VirtualProtect to set memory protection based on global 'how'
	 * array; 'how' array keeps protection level for each page
	 */
	p = VirtualAlloc(NULL, 3 * pageSize, MEM_COMMIT, PAGE_NOACCESS);
	DIE(p == NULL, "VirtualAlloc");

	rc = VirtualProtect(p, pageSize, how[0], &old);
	DIE(rc == FALSE, "VirtualProtect");
	rc = VirtualProtect(p + pageSize, pageSize, how[1], &old);
	DIE(rc == FALSE, "VirtualProtect");
	rc = VirtualProtect(p + 2 * pageSize, pageSize, how[2], &old);
	DIE(rc == FALSE, "VirtualProtect");

	set_signal();

	// BEFORE exception handler
	// read
	/*
	ch = p[0]; // crash
	printf("%c\n", ch);
	ch = p[pageSize]; // ok
	printf("%c\n", ch);
	ch = p[2 * pageSize]; // ok
	printf("%c\n", ch);
	*/
	// write
	/*
	p[0*pageSize] = 'a'; // crash
	p[1*pageSize] = 'a'; // crash
	p[2*pageSize] = 'a'; // ok
	*/
	
	// AFTER exception handler
	// read
	ch = p[0]; // page 0 gets PAGE_READONLY
	printf("%c\n", ch);
	ch = p[pageSize]; // ok
	printf("%c\n", ch);
	ch = p[2 * pageSize]; // ok
	printf("%c\n", ch);
	// write
	p[0*pageSize] = 'a'; // page 0 gets PAGE_READWRITE
	p[1*pageSize] = 'a'; // page 1 gets PAGE_READWRITE
	p[2*pageSize] = 'a'; // ok

	restore_signal();

	/* TODO 1 - cleanup */
	//rc = VirtualFree(p, 3 * pageSize, MEM_DECOMMIT);
	rc = VirtualFree(p, 0, MEM_RELEASE);
	DIE(rc == FALSE, "VirtualFree");

	return 0;
}
