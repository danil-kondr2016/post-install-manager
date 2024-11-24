#include "operations.h"
#include "utils.h"

#include <string.h>
#include <wchar.h>
#include "sds.h"
#include "cwalk.h"

static HRESULT copy_recursively(LPWSTR Source, LPWSTR Destination);

HRESULT OpCopyDirectory(char *Source, char *Destination)
{
	HRESULT result = S_OK;
	LPWSTR szwSource, szwDestination;
	char *BaseName;

	szwSource = UTF8ToWideCharAlloc(Source);
	if (!szwSource) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	cwk_path_get_basename(Source, &BaseName, NULL);
	if (TestIsDirectory(Destination)) {
		sds NewDestination = sdsempty();
		size_t NewDestinationSize;

		NewDestinationSize = cwk_path_get_absolute(Destination, BaseName, NewDestination, 0);
		sdsgrowzero(NewDestination, NewDestinationSize + 1);
		cwk_path_get_absolute(Destination, BaseName, NewDestination, NewDestinationSize);

		szwDestination = UTF8ToWideCharAlloc(NewDestination);
		sdsfree(NewDestination);
	}
	else {
		szwDestination = UTF8ToWideCharAlloc(Destination);
	}

	if (!szwDestination) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}

	result = copy_recursively(szwSource, szwDestination);

cleanup2:
	HeapFree(GetProcessHeap(), 0, szwDestination);
cleanup1:
	HeapFree(GetProcessHeap(), 0, szwSource);
exit_fn:
	return result;
}

static LPWSTR append_alloc(LPWSTR Path1, LPWSTR Path2)
{
	size_t Path1Size, Path2Size;
	size_t NewPathSize;
	LPWSTR NewPath;

	Path1Size = wcslen(Path1);
	Path2Size = wcslen(Path2);
	NewPathSize = Path1Size + Path2Size + 1;

	NewPath = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (NewPathSize + 1) * sizeof(WCHAR));
	if (!NewPath)
		return NULL;

	wcsncpy_s(NewPath, NewPathSize, Path1, Path1Size);
	NewPath[Path1Size] = L'\0';
	wcsncat_s(NewPath, NewPathSize, L"\\", 1);
	wcsncat_s(NewPath, NewPathSize, Path2, Path2Size);
	NewPath[NewPathSize] = L'\0';

	return NewPath;
}

static HRESULT copy_recursively(LPWSTR Source, LPWSTR Destination)
{
	size_t SourceLength, DestinationLength;
	WIN32_FIND_DATAW FindData = { 0 };
	HANDLE hFind;
	HRESULT result = S_OK;

	if (!CreateDirectoryW(Destination, NULL)) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	SourceLength = wcslen(Source);
	DestinationLength = wcslen(Destination);

	hFind = FindFirstFileW(Source, &FindData);
	do {
		LPWSTR NewSource, NewDestination;

		if (FindData.cFileName[0] == '.' && !FindData.cFileName[1])
			continue;
		if (FindData.cFileName[0] == '.' && FindData.cFileName[1] == '.' && !FindData.cFileName[2])
			continue;

		NewSource = append_alloc(Source, FindData.cFileName);
		if (!NewSource) {
			result = E_OUTOFMEMORY;
			goto cleanup1;
		}

		NewDestination = append_alloc(Destination, FindData.cFileName);
		if (!NewDestination) {
			result = E_OUTOFMEMORY;
			goto cleanup2;
		}

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			result = copy_recursively(NewSource, NewDestination);
		}
		else {
			CopyFileW(NewSource, NewDestination, FALSE);
			result = HRESULT_FROM_WIN32(GetLastError());
		}

	cleanup3:
		HeapFree(GetProcessHeap(), 0, NewDestination);
	cleanup2:
		HeapFree(GetProcessHeap(), 0, NewSource);
	}
	while (SUCCEEDED(result) && FindNextFileW(hFind, &FindData));

cleanup1:
	FindClose(hFind);
	return result;
}