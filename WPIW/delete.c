#include "operations.h"
#include "utils.h"
#include "tests.h"

#include <windows.h>
#include <string.h>
#include "sds.h"
#include "cwalk.h"

HRESULT OpRemoveFile(char* File)
{
	HRESULT result = S_OK;
	LPWSTR szwFile;
	char* BaseName;

	szwFile = UTF8ToWideCharAlloc(File);
	if (!szwFile) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	DeleteFileW(szwFile);
	result = HRESULT_FROM_WIN32(GetLastError());

	HeapFree(GetProcessHeap(), 0, szwFile);
exit_fn:
	return result;
}