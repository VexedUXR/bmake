/* Interrupt handling */

#include "make.h"
#include "job.h"

static HWND window; /* Our invisible message - only window. */
static HANDLE thread; /* Used to suspend the main thread. */
static void (*IntProc)(void);
static void (*TermProc)(void);

static HANDLE msgThread; /* This is only here for cleanup */

static BOOL WINAPI
MsgConsoleH(DWORD ev)
{
	void (*proc)(void);

	switch (ev) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		proc = IntProc;
		break;
	case CTRL_CLOSE_EVENT:
		proc = TermProc;
		break;
	default:
		return FALSE;
	}

	/*
	 * Avoid possible deadlock caused by the
	 * suspended thread holding a mutex.
	 */
	Job_WaitMutex();
	if (SuspendThread(thread) == -1)
		Punt("failed to suspend main thread: %s",
			strerr(GetLastError()));
	proc();
	if (ResumeThread(thread) == -1)
		Punt("failed to resume main thread: %s",
			strerr(GetLastError()));
	Job_ReleaseMutex();

	return TRUE;
}

static LRESULT
MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CLOSE:
		/* The same as CTRL-C */
		MsgConsoleH(CTRL_C_EVENT);
		return 0;
	default:
		return DefWindowProcA(hWnd, uMsg, wParam, lParam);
	}
}

/*
 * Create a new window then go into
 * the message loop, notifying the calling
 * thread when we are ready.
 */
static DWORD WINAPI
MsgLoop(LPVOID ready)
{
	MSG msg;
	BOOL ret;
	WNDCLASSA wClass = { 0 };

	wClass.lpszClassName = progname;
	wClass.lpfnWndProc = MsgProc;
	wClass.hInstance = GetModuleHandleA(NULL);

	if (RegisterClassA(&wClass) == 0)
		Punt("failed to register class: %s", strerr(GetLastError()));
	if ((window = CreateWindowExA(0, progname, NULL, 0, 0, 0, 0, 0,
		HWND_MESSAGE, NULL, wClass.hInstance, NULL)) == NULL)
		Punt("failed to create window: %s", strerr(GetLastError()));

	*(bool *)ready = true;

	while ((ret = GetMessageA(&msg, window, 0, 0)) != 0) {
		if (ret == -1)
			Punt("failed to get message: %s", strerr(GetLastError()));

		DispatchMessageA(&msg);
	}

	return 0;
}

/*
 * Get the handle for the calling thread,
 * set the ctrl handler then start a
 * new thread at MsgLoop.
 *
 * Input:
 *	intProc	non-fatal interrupt handler
 *	termProc fatal interrupt handler
 */
void
Msg_Init(void (*intProc)(void), void (*termProc)(void))
{
	volatile bool ready = false;

	IntProc = intProc;
	TermProc = termProc;

	{
		HANDLE process = GetCurrentProcess();

		if (DuplicateHandle(process, GetCurrentThread(), process,
			&thread, 0, FALSE, DUPLICATE_SAME_ACCESS) == 0)
			Punt("failed to duplicate thread handle: %s",
				strerr(GetLastError()));
	}

	if (SetConsoleCtrlHandler(MsgConsoleH, TRUE) == 0)
		Punt("failed to set console ctrl handler: %s", strerr(GetLastError()));
	if ((msgThread = CreateThread(NULL, 0, MsgLoop, &ready, 0, NULL)) == NULL)
		Punt("failed to start msg thread: %s", strerr(GetLastError()));

	while (!ready);
}

void Msg_End(void)
{
#ifdef CLEANUP
	PostMessageA(window, WM_QUIT, 0, 0);
	SetConsoleCtrlHandler(MsgConsoleH, FALSE);
	CloseHandle(thread);
	CloseHandle(msgThread);
#endif
}
