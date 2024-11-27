#include "utils.h"
#include "sds.h"

#include <windows.h>

wchar_t *UTF8ToWideCharAlloc(char *source)
{
	DWORD resultSize;
	wchar_t *result;
	int i;

	resultSize = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
	result = (wchar_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(wchar_t) * (resultSize + 1));
	if (!result)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, source, -1, result, resultSize);
	result[resultSize] = 0;

	return result;
}

sds WideCharToSdsAlloc(wchar_t *source)
{
	DWORD resultSize;
	sds result;

	resultSize = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
	result = sdsgrowzero(sdsempty(), resultSize + 1);
	if (!result)
		return NULL;
	WideCharToMultiByte(CP_UTF8, 0, source, -1, result, resultSize, NULL, NULL);
	sdsupdatelen(result);

	return result;
}