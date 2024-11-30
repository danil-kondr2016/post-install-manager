#include "stdafx.h"

bool TestExists(char *Path)
{
	HANDLE hFind;
	WIN32_FIND_DATA findData;
	bool result = false;

	hFind = FindFirstFile(Path, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
		result = true;

	FindClose(hFind);
	return result;
}

bool TestIsDirectory(char *Path)
{
	DWORD Attributes;
	bool result = false;

	Attributes = GetFileAttributesW(Path);
	if (Attributes != INVALID_FILE_ATTRIBUTES && Attributes & FILE_ATTRIBUTE_DIRECTORY) {
		result = true;
	}

	return result;
}
