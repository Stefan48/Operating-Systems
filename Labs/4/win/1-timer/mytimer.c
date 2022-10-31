/**
 * SO, 2016 - Lab #4, Signals
 * Task #1, Windows
 *
 * Timer usage.
 */

#define _WIN32_WINNT	0x0500

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include <windows.h>
#include "utils.h"

#define _SECOND		10000000
#define TIMEOUT		(-2 * _SECOND)


/*
 * Timer APC routine
 */
static VOID CALLBACK TimerAPCProc(LPVOID lpArgToCompletionRoutine,
		DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	/* TODO: Use ctime and time to print current time. */
	time_t currentTime;
	
	currentTime = time(NULL);
	printf("%s", ctime(&currentTime));
}

/*
 * Initialise and configure timer
 */
static void InitSetTimer(void)
{
	/*
	 * TODO
	 * First, create your timer using CreateWaitableTimer.
	 * Then set its timeout and its routine using SetWaitableTimer.
	 * No data needs to be passeed to the completion routine.
	 * Don't forget to check those calls for errors.
	 */
	HANDLE hTimer;
	LARGE_INTEGER liDueTime;
	BOOL bRet;
	
	hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
	DIE(hTimer == NULL, "CreateWaitableTimer");
	
	liDueTime.QuadPart = TIMEOUT;
	bRet = SetWaitableTimer(hTimer,
							&liDueTime,
							2000,
							TimerAPCProc,
							NULL,
							FALSE);
}

int main(void)
{
	InitSetTimer(); /* configures the timer to 2 seconds */

	while (1) {
		/* TODO: Wait for timer (use SleepEx function). */
		SleepEx(INFINITE, TRUE);
	}

	return 0;
}
