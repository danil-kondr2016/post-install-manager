#include "tests.h"
#include "utils.h"

#include <windows.h>
#include <string.h>

#include "sds.h"
#include "cwalk.h"

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
