/* do not use UNICODE */
#undef _UNICODE
#undef UNICODE

#include <windows.h>
#include <stdio.h>
#include "utils.h"

#define NO_THREADS	5

typedef struct {
	HANDLE hGuard;	   /* mutex to protect internal variable access */
	HANDLE hEvent;	   /* auto-resatable event */
	DWORD dwCount;	   /* number of threads to have reached the barrier */
	DWORD dwThreshold; /* barrier limit */
} THRESHOLD_BARRIER, *THB_OBJECT;

THB_OBJECT barrier; /* global barrier */

DWORD CreateThresholdBarrier(THB_OBJECT *pthb, DWORD b_value)
{
	THB_OBJECT hthb;

	/* Initialize a barrier object */
	hthb = malloc(sizeof(THRESHOLD_BARRIER));
	*pthb = hthb;
	if (hthb == NULL) 
		return FALSE;

	/* TODO - Create Mutex */
	hthb->hGuard = CreateMutex(
		NULL,  /* default security attributes */
		FALSE, /* initially not owned */
		NULL   /* unnamed mutex */
	);
	DIE(hthb->hGuard == NULL, "CreateMutex");

	/* TODO - Create Event */
	hthb->hEvent = CreateEvent(
          NULL,
          TRUE,   /* Manual Reset */
          FALSE,  /* Non-signaled state */
          NULL    /* Private variable */
	);
	DIE(hthb->hEvent == NULL, "CreateEvent");
	
	/* set maximum number of waiting threads */
	hthb->dwThreshold = b_value;
	
	/* set number of waiting threads to 0 */
	hthb->dwCount = 0;

	return TRUE;
}


DWORD WaitThresholdBarrier(THB_OBJECT thb)
{
	/* TODO - Wait for the specified number of thread to reach */
	/* the barrier, then broadcast the release to all waiting threads */

	DWORD dwRet;
	BOOL bRet;

	dwRet = WaitForSingleObject(thb->hGuard, INFINITE);
	DIE(dwRet == WAIT_FAILED, "WaitForSingleObject");
	thb->dwCount++;
	if (thb->dwCount == thb->dwThreshold)
	{
		thb->dwCount = 0;
		bRet = ReleaseMutex(thb->hGuard);
		DIE(bRet == FALSE, "ReleaseMutex");
		bRet = PulseEvent(thb->hEvent);
		DIE(bRet == FALSE, "PulseEvent");
	}
	else
	{
		dwRet = SignalObjectAndWait(thb->hGuard, thb->hEvent, INFINITE, FALSE);
		DIE(dwRet == WAIT_FAILED, "SignalObjectAndWait");

		/*bRet = ReleaseMutex(thb->hGuard);
		DIE(bRet == FALSE, "ReleaseMutex");
		dwRet = WaitForSingleObject(thb->hEvent, INFINITE);
		DIE(dwRet != WAIT_OBJECT_0, "WaitForSingleObject");*/
	}

	return 0;
}

DWORD CloseThresholdBarrier(THB_OBJECT thb)
{
	CloseHandle(thb->hEvent);
	CloseHandle(thb->hGuard);
	free(thb);

	return 0;
}

DWORD WINAPI ThreadFunc(LPVOID lpParameter)
{
	DWORD work_time = 0;
	DWORD tid = GetCurrentThreadId();

	printf("Thread %d starts working\n", tid);

	/* Do work */
	work_time = tid;
	Sleep(work_time);

	/* Sync with others */
	WaitThresholdBarrier(barrier);

	printf("Thread %d exited the barrier. Start working again\n", tid);

	/* Do some more work */
	work_time = tid;
	Sleep(work_time);

	/* Sync with others */
	WaitThresholdBarrier(barrier);

	printf("Thread %d exited the barrier\n", tid);

	return 0;
}


int main(VOID)
{
	DWORD dwRet, IDThread;
	HANDLE hThread[NO_THREADS];
	int i;

	setbuf(stdout, NULL);

	dwRet = CreateThresholdBarrier(&barrier, NO_THREADS);
	DIE(dwRet == FALSE, "CreateThresholdBarrier failed");

	/* create threads */
	for (i = 0; i < NO_THREADS; i++) {
		hThread[i] = CreateThread(
				NULL,   /* default security attributes */
				0,      /* default stack size */
				(LPTHREAD_START_ROUTINE) ThreadFunc,
				NULL,   /* no thread parameter */
				0,      /* immediately run the thread */
				&IDThread);     /* thread id */
		DIE(hThread[i] == NULL, "CreateThread");
	}

	/* wait for threads completion */
	for (i = 0; i < NO_THREADS; i++) {
		dwRet = WaitForSingleObject(hThread[i], INFINITE);
		DIE(dwRet == WAIT_FAILED, "WaitForSingleObject");
	}

	CloseThresholdBarrier(barrier);

	return 0;
}
