/* do not use UNICODE */
#undef _UNICODE
#undef UNICODE

#define _WIN32_WINNT    0x500
#include <windows.h>
#include <stdio.h>

#include "utils.h"

#define PERIOD       1000
#define TIMES        3

HANDLE finished;

VOID CALLBACK TimerFunction(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	static int count;
	BOOL bRet;

	printf("'TimerFunction' has been called and count is %d\n", count);

	/* TODO - Check if we must increment counter or finish */
	if (count == TIMES - 1)
	{
		bRet = SetEvent(finished);
		DIE(bRet == FALSE, "SetEvent");
	}
	else
	{
		count++;
	}
}

int main(void)
{
	HANDLE timer_queue;
	HANDLE timer;
	BOOL bRet;
	DWORD dwRet;

	setbuf(stdout, NULL);

	/* TODO - Create a TimerQueue */
	timer_queue = CreateTimerQueue();
	DIE(timer_queue == NULL, "CreateTimerQueue");

	/* TODO - Create a semaphore/event */
	finished = CreateEvent(
          NULL,
          FALSE,  /* Auto Reset */
          FALSE,  /* Non-signaled state */
          NULL    /* Private variable */
	);
	DIE(finished == NULL, "CreateEvent");

	/* TODO - Create a timer and associate it with the timer queue */
	bRet = CreateTimerQueueTimer(
				&timer,         /* handle for the new timer */
				timer_queue,    /* timer queue */
				TimerFunction,  /* callback to execute */
				NULL,           /* callback parameter */
				0,              /* timer first expires after 0 milisec */
				PERIOD,         /* then timer expires repeatedly after PERIOD milisec */
				0               /* callback type: IO/NonIO, EXECUTEONLYONCE etc. */
	);
	DIE(bRet == FALSE, "CreateTimerQueueTimer");

	/* TODO - Wait for the semaphore/event to be set so we can free resources */
	dwRet = WaitForSingleObject( 
        finished,  /* event handle */
        INFINITE   /* indefinite wait */
	);
	DIE(dwRet != WAIT_OBJECT_0, "WaitForSingleObject");

	/* TODO - Delete the timer queue and its timers */
	bRet = DeleteTimerQueueTimer(timer_queue, timer, INVALID_HANDLE_VALUE);
	DIE(bRet == FALSE, "DeleteTimerQueueTimer");

	bRet = DeleteTimerQueueEx(timer_queue, INVALID_HANDLE_VALUE);
	DIE(bRet == FALSE, "DeleteTimerQueueEx");

	return 0;
}
