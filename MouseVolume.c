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

typedef struct tagMV_MouseMappingArray {
	DWORD length;
	MV_MouseMapping* item[0];
} *MV_MouseMappingArray;

BOOL MV_InitMouseMap(MV_MouseMappingArray* arrPtr) {
	if (arrPtr == NULL) {
		return FALSE;
	}

	DWORD proposedLength = sizeof(struct tagMV_MouseMappingArray);
	MV_MouseMappingArray allocResult = HeapAlloc(GetProcessHeap(), HEAP_NO_SERIALIZE, proposedLength);

	if (allocResult == NULL) {
		return FALSE;
	}

	allocResult->length = 0;

	*arrPtr = allocResult;

	return TRUE;
}

BOOL MV_AddMouseMap(MV_MouseMappingArray* arrPtr, WORD button, WORD vk) {
	if (arrPtr == NULL) {
		return FALSE;
	}

	MV_MouseMappingArray arr = *arrPtr;

	if (arr == NULL) {
		return FALSE;
	}

	HANDLE hProcessHeap = GetProcessHeap();

	// Create mouse mapping
	MV_MouseMapping* mapping = HeapAlloc(hProcessHeap, HEAP_NO_SERIALIZE, sizeof(MV_MouseMapping));

	if (mapping == NULL) {
		return FALSE;
	}

	mapping->delayTimer = 0;
	mapping->repeatTimer = 0;
	mapping->button = button;
	mapping->vk = vk;

	DWORD proposedLength = arr->length + 1;
	DWORD proposedSize = proposedLength * sizeof(MV_MouseMapping*) + sizeof(struct tagMV_MouseMappingArray);

	LPVOID reallocResult = HeapReAlloc(hProcessHeap, HEAP_NO_SERIALIZE, arr, proposedSize);

	if (reallocResult == NULL) {
		// Free created mapping
		HeapFree(hProcessHeap, HEAP_NO_SERIALIZE, mapping);

		return FALSE;
	}

	arr = reallocResult;
	*arrPtr = reallocResult;

	arr->item[arr->length] = mapping;
	arr->length = proposedLength;

	return TRUE;
}

BOOL MV_FreeMouseMap(MV_MouseMappingArray* arrPtr) {
	if (arrPtr == NULL) {
		return FALSE;
	}

	MV_MouseMappingArray arr = *arrPtr;

	if (arr == NULL) {
		return TRUE;
	}

	HANDLE hProcessHeap = GetProcessHeap();

	for (int i = 0; i < arr->length; i++) {
		MV_MouseMapping* item = arr->item[i];

		if (item == NULL) {
			continue;
		}

		if (item->delayTimer != 0) {
			KillTimer(NULL, item->delayTimer);

			item->delayTimer = 0;
		}

		if (item->repeatTimer != 0) {
			KillTimer(NULL, item->repeatTimer);

			item->repeatTimer = 0;
		}

		HeapFree(hProcessHeap, HEAP_NO_SERIALIZE, item);

		arr->item[i] = NULL;
	}

	HeapFree(hProcessHeap, HEAP_NO_SERIALIZE, arr);

	*arrPtr = NULL;

	return TRUE;
}

MV_MouseMappingArray mouseMaps = NULL;

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
	for (int i = 0; i < mouseMaps->length; i++) {
		MV_MouseMapping* item = mouseMaps->item[i];

		if (item == NULL) {
			continue;
		}

		if (item->delayTimer == idEvent || item->repeatTimer == idEvent) {
			// Determine delay state
			if (item->delayTimer == idEvent) {
				KillTimer(NULL, item->delayTimer);

				item->delayTimer = 0;

				// Get repeat rate
				DWORD dwKbdRepeatRate;

				SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &dwKbdRepeatRate, 0);

				UINT kbdRepeatInterval = (UINT)fmap(dwKbdRepeatRate, 0.0, 31.0, 400.0, 33.0);

				// Init repeat timer
				if (item->repeatTimer == 0) {
					item->repeatTimer = SetTimer(NULL, 0, kbdRepeatInterval, MouseMappingTimerProc);
				}
			}

			// Repeat state
			if (item->repeatTimer == idEvent) {
				SendVk(item->vk);
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
			for (int i = 0; i < mouseMaps->length; i++) {
				MV_MouseMapping* item = mouseMaps->item[i];

				if (item == NULL) {
					continue;
				}

				if (item->button != button) {
					continue;
				}

				if (wParam == WM_XBUTTONUP) {
					cancel = TRUE;

					if (item->delayTimer != 0) {
						KillTimer(NULL, item->delayTimer);

						item->delayTimer = 0;
					}

					if (item->repeatTimer != 0) {
						KillTimer(NULL, item->repeatTimer);

						item->repeatTimer = 0;
					}
				} else if (wParam == WM_XBUTTONDOWN) {
					cancel = TRUE;

					// Find keyboard repeat delay
					int kbdDelay;

					SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &kbdDelay, 0);

					UINT kbdDelayTime = (UINT)fmap(kbdDelay, 0.0, 3.0, 250.0, 1000.0);

					// Init delay timer
					if (item->delayTimer == 0) {
						item->delayTimer = SetTimer(NULL, 0, kbdDelayTime, MouseMappingTimerProc);

						// Send single VK
						SendVk(item->vk);
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

	// Create mutex if it doesn't exist
	if (hMutex == NULL) {
		DWORD dwLastError = GetLastError();

		if (dwLastError == ERROR_FILE_NOT_FOUND) {
			hMutex = CreateMutex(NULL, FALSE, TEXT("MouseVolume.0"));
		} else {
			return dwLastError;
		}
	}

	if (!MV_InitMouseMap(&mouseMaps)) {
		ReleaseMutex(hMutex);

		return GetLastError();
	}

	MV_AddMouseMap(&mouseMaps, 1, VK_VOLUME_DOWN);
	MV_AddMouseMap(&mouseMaps, 2, VK_VOLUME_UP);

	if (mouseMaps->length == 0) {
		ReleaseMutex(hMutex);

		return GetLastError();
	}

	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

	if (mouseHook == NULL) {
		ReleaseMutex(hMutex);

		return GetLastError();
	}

	// Message loop
	MSG msg;

	msg.message = 0;

	BOOL bGetMessageResult;
	DWORD dwGetMessageError;

	while ((bGetMessageResult = GetMessage(&msg, NULL, 0, 0)) != -1) {
		if (msg.message == WM_QUIT) {
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		Sleep(0);
	}

	if (bGetMessageResult == -1) {
		dwGetMessageError = GetLastError();
	}

	MV_FreeMouseMap(&mouseMaps);

	UnhookWindowsHookEx(mouseHook);

	ReleaseMutex(hMutex);

	if (bGetMessageResult != -1) {
		return dwGetMessageError;
	} else {
		return 0;
	}
}

void __stdcall WinMainCRTStartup() {
	int result = WinMain(GetModuleHandle(NULL), 0, GetCommandLineA(), SW_SHOWDEFAULT);

	ExitProcess(result);
}
