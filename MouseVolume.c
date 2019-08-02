#define UNICODE
#define _UNICODE

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	int _fltused = 0;
#ifdef __cplusplus
}
#endif

HHOOK mouseHook;

typedef struct tagMV_MouseMapping {
	UINT_PTR delayTimer;
	UINT_PTR repeatTimer;
	WORD button;
	WORD vk;
} MV_MouseMapping;

#define MOUSEMAP_COUNT 2
#define MOUSEMAP_VOLUME_DN 0
#define MOUSEMAP_VOLUME_UP 1

MV_MouseMapping mouseMaps[MOUSEMAP_COUNT];

void initHooks() {
	mouseMaps[MOUSEMAP_VOLUME_DN].delayTimer = 0;
	mouseMaps[MOUSEMAP_VOLUME_DN].repeatTimer = 0;
	mouseMaps[MOUSEMAP_VOLUME_DN].button = 1;
	mouseMaps[MOUSEMAP_VOLUME_DN].vk = VK_VOLUME_DOWN;

	mouseMaps[MOUSEMAP_VOLUME_UP].delayTimer = 0;
	mouseMaps[MOUSEMAP_VOLUME_UP].repeatTimer = 0;
	mouseMaps[MOUSEMAP_VOLUME_UP].button = 2;
	mouseMaps[MOUSEMAP_VOLUME_UP].vk = VK_VOLUME_UP;
}

double fmap(double x, double from1, double from2, double to1, double to2) {
	return (x - from1) / (from2 - from1) * (to2 - to1) + to1;
}

VOID SendVk(WORD vk) {
	// Init input
	INPUT input;

	input.type = INPUT_KEYBOARD;
	input.ki.wScan = 0;
	input.ki.time = 0;
	input.ki.dwExtraInfo = 0;

	input.ki.wVk = vk;

	// Key down
	input.ki.dwFlags = 0;

	SendInput(1, &input, sizeof(INPUT));

	// Key up
	input.ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(1, &input, sizeof(INPUT));
}

VOID CALLBACK MouseMappingTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	// Find mapping
	for (int i = 0; i < MOUSEMAP_COUNT; i++) {
		if (mouseMaps[i].delayTimer == idEvent || mouseMaps[i].repeatTimer == idEvent) {
			MV_MouseMapping* mapping = &(mouseMaps[i]);

			// Determine delay state
			if (mapping->delayTimer == idEvent) {
				KillTimer(NULL, mapping->delayTimer);

				mapping->delayTimer = 0;

				// Get repeat rate
				DWORD kbdRepeatRate;

				SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &kbdRepeatRate, 0);

				UINT kbdRepeatInterval = (UINT)fmap(kbdRepeatRate, 0.0, 31.0, 400.0, 33.0);

				// Init repeat timer
				mapping->repeatTimer = SetTimer(NULL, 0, kbdRepeatInterval, MouseMappingTimerProc);
			}

			// Repeat state
			if (mapping->repeatTimer == idEvent) {
				SendVk(mapping->vk);
			}
		}
	}
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		PMSLLHOOKSTRUCT data = (PMSLLHOOKSTRUCT)lParam;

		if (wParam == WM_XBUTTONDOWN || wParam == WM_XBUTTONUP) {
			WORD button = HIWORD(data->mouseData);
			BOOL cancel = FALSE;

			// Find mapping for button
			for (int i = 0; i < MOUSEMAP_COUNT; i++) {
				if (mouseMaps[i].button == button) {
					cancel = TRUE;

					MV_MouseMapping* mapping = &(mouseMaps[i]);

					// Cancel timers if they're active
					if (wParam == WM_XBUTTONUP) {
						if (mapping->delayTimer != 0) {
							KillTimer(NULL, mapping->delayTimer);

							mapping->delayTimer = 0;
						}

						if (mapping->repeatTimer != 0) {
							KillTimer(NULL, mapping->repeatTimer);

							mapping->repeatTimer = 0;
						}
					}
					else {
						// Find keyboard repeat delay
						int kbdDelay;

						SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &kbdDelay, 0);

						UINT kbdDelayTime = (UINT)fmap(kbdDelay, 0.0, 3.0, 250.0, 1000.0);

						// Init delay timer
						mapping->delayTimer = SetTimer(NULL, 0, kbdDelayTime, MouseMappingTimerProc);

						// Send single VK
						SendVk(mapping->vk);
					}
				}
			}

			if (cancel) {
				return -1;
			}
		}
	}

	return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// Only one instance
	HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("MouseVolume.0"));

	if (!hMutex) {
		hMutex = CreateMutex(NULL, FALSE, TEXT("MouseVolume.0"));
	} else {
		return 0;
	}

	initHooks();

	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

	// Message loop
	MSG msg;

	msg.message = 0;

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message == WM_QUIT) {
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		Sleep(0);
	}

	// Cleanup
	for (int i = 0; i < MOUSEMAP_COUNT; i++) {
		// Kill timers
		if (mouseMaps[i].delayTimer != 0) {
			KillTimer(NULL, mouseMaps[i].delayTimer);

			mouseMaps[i].delayTimer = 0;
		}

		if (mouseMaps[i].repeatTimer != 0) {
			KillTimer(NULL, mouseMaps[i].repeatTimer);

			mouseMaps[i].repeatTimer = 0;
		}
	}

	UnhookWindowsHookEx(mouseHook);

	ReleaseMutex(hMutex);

	return 0;
}

void __stdcall WinMainCRTStartup() {
	int result = WinMain(GetModuleHandle(NULL), 0, GetCommandLineA(), SW_SHOWDEFAULT);

	ExitProcess(result);
}
