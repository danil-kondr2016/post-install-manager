#include "utils.h"

#include <windows.h>

wchar_t *UTF8ToWideCharAlloc(char *source)
{
	DWORD resultSize;
	wchar_t *result;
	int i;

	resultSize = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
	result = (wchar_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(wchar_t) * resultSize);
	if (!result)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, source, -1, result, resultSize);
	result[resultSize] = 0;

	return result;
}