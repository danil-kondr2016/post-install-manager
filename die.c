#include "die.h"

#include <stdarg.h>

void FatalError(HRESULT Result, ...)
{
	WCHAR Buffer[4096];
	va_list vl;

	va_start(vl, Result);
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Result, LANG_NEUTRAL | SUBLANG_DEFAULT, Buffer, 4096, &vl);
	va_end(vl);

	MessageBoxW(NULL, Buffer, NULL, MB_ICONHAND | MB_OK);
	ExitProcess(1);
}