#include "fatal.h"

#include <windows.h>

void fatal_errorW(wchar_t *message, uint32_t exit_code)
{
	MessageBoxW(NULL, message, NULL, MB_ICONHAND | MB_SYSTEMMODAL);
	ExitProcess(exit_code);
}
