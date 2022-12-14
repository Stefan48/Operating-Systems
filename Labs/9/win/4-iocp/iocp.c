/*
 * SO, 2016 - Lab 10 (Advanced I/O Windows
 * I/O completion ports wrapper functions
 * Task #4 (I/O completion ports)
 */

#include <windows.h>

#include "iocp.h"

/*
 * create I/O completion port
 */

HANDLE iocp_init(void)
{
	/* TODO */
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR) NULL, 0);
}

/*
 * add handle to completion port; use handle as key
 */

HANDLE iocp_add(HANDLE iocp, HANDLE hFile)
{
	/* TODO */
	return CreateIoCompletionPort(hFile, iocp, (ULONG_PTR) hFile /* use handle as key */, 0);
}

/*
 * add handle to completion port; use key as key
 */

HANDLE iocp_add_key(HANDLE iocp, HANDLE hFile, ULONG_PTR key)
{
	/* TODO */
	return CreateIoCompletionPort(hFile, iocp, key, 0);
}

/*
 * wait for event notification on completion port
 */

BOOL iocp_wait(HANDLE iocp, PDWORD bytes, PULONG_PTR key, LPOVERLAPPED *op)
{
	/* TODO */
	return GetQueuedCompletionStatus(
         iocp,    /* completion port handle */
         bytes,   /* actual bytes transferred */
         key,     /* return key to indentify the operation */
         op,      /* overlapped structure used */
         INFINITE /* wait time */
	);
}
