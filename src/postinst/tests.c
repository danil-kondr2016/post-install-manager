#include "tests.h"

#include <windows.h>

bool test_existts(char *path, struct arena scratch)
{
	HANDLE hFind;
	WIN32_FIND_DATAW findData;
	bool result = false;
	wchar_t *pathW;

	pathW = u8_to_u16(path, &scratch);
	hFind = FindFirstFileW(pathW, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
		result = true;

	FindClose(hFind);
	return result;
}

bool test_is_directory(char *path, struct arena scratch)
{
	DWORD Attributes;
	bool result = false;
	wchar_t *pathW = u8_to_u16(path, &scratch);

	Attributes = GetFileAttributes(pathW);
	if ((Attributes != INVALID_FILE_ATTRIBUTES) && (Attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		result = true;
	}

	return result;
}
