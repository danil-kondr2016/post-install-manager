#include "fatal.h"

#include <windows.h>

void fatal_errorW(wchar_t *message, uint32_t exit_code)
{
	MessageBoxW(NULL, message, NULL, MB_ICONHAND | MB_SYSTEMMODAL);
	ExitProcess(exit_code);
}

void error_msgW(void *window, uint32_t result)
{
	HWND hWnd = (HWND)window;
	HMODULE hNtdll;
	WCHAR buffer[1024] = {0};

	hNtdll = GetModuleHandleA("ntdll.dll");
	if ((result & 0x0FFF0000) == 0x00070000) {
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			(result & 0xFFFF),
			0,
			buffer,
			1024,
			NULL);
	}
	else if ((result & 0x2FF00000) == 0x27F00000) {
		FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE, NULL,
			result,
			0,
			buffer,
			1024,
			NULL);
	}
	else {
		FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE, hNtdll,
			result,
			0,
			buffer,
			1024,
			NULL);
	}

	MessageBoxW(hWnd, buffer, NULL, MB_ICONHAND);
}
