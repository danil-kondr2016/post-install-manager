#include "tests.h"
#include "utils.h"

#include <windows.h>
#include <string.h>

#include "sds.h"
#include "cwalk.h"

bool TestExists(char *Path)
{
	HANDLE hFind;
	LPWSTR szwPath;
	WIN32_FIND_DATAW findData;
	bool result = false;

	szwPath = UTF8ToWideCharAlloc(Path);
	if (!szwPath)
		return false;

	hFind = FindFirstFileW(szwPath, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
		result = true;

	FindClose(hFind);
	return result;
}

bool TestIsDirectory(char *Path)
{
	DWORD Attributes;
	LPWSTR szwPath;
	bool result = false;

	szwPath = UTF8ToWideCharAlloc(Path);
	if (!szwPath) {
		return false;
	}

	Attributes = GetFileAttributesW(szwPath);
	if (Attributes == INVALID_FILE_ATTRIBUTES) {
		result = false;
	}
	else if (Attributes & FILE_ATTRIBUTE_DIRECTORY) {
		result = true;
	}

	HeapFree(GetProcessHeap(), 0, szwPath);
exit_fn:
	return result;
}
