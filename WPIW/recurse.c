#include "operations.h"
#include "utils.h"
#include "tests.h"

#include <string.h>
#include <wchar.h>
#include "sds.h"
#include "cwalk.h"

typedef HRESULT (*file_callback)(LPWSTR arg1, LPWSTR arg2);

static HRESULT copy_single(LPWSTR Source, LPWSTR Destination)
{
	if (!CopyFileW(Source, Destination, FALSE))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT delete_single(LPWSTR File, LPWSTR Unused)
{
	(void)Unused;
	if (!DeleteFileW(File))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT create_destination(LPWSTR Unused, LPWSTR Destination)
{
	(void)Unused;
	if (!CreateDirectoryW(Destination, NULL))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT delete_folder(LPWSTR Source, LPWSTR Unused)
{
	(void)Unused;
	if (!RemoveDirectoryW(Source))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT enum_recursively(LPWSTR Source, LPWSTR Destination, file_callback begin_cb, file_callback cb, file_callback end_cb);

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

		NewDestinationSize = cwk_path_join(Destination, BaseName, NewDestination, 0);
		NewDestination = sdsgrowzero(NewDestination, NewDestinationSize + 1);
		cwk_path_join(Destination, BaseName, NewDestination, NewDestinationSize);

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

	result = enum_recursively(szwSource, szwDestination, create_destination, copy_single, NULL);

cleanup2:
	HeapFree(GetProcessHeap(), 0, szwDestination);
cleanup1:
	HeapFree(GetProcessHeap(), 0, szwSource);
exit_fn:
	return result;
}

HRESULT OpRemoveDirectory(char* Source)
{
	HRESULT result = S_OK;
	LPWSTR szwSource;
	char* BaseName;

	szwSource = UTF8ToWideCharAlloc(Source);
	if (!szwSource) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	result = enum_recursively(szwSource, NULL, NULL, delete_single, delete_folder);
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

	wcsncpy_s(NewPath, NewPathSize + 1, Path1, Path1Size);
	NewPath[Path1Size] = L'\0';
	wcsncat_s(NewPath, NewPathSize + 1, L"\\", 1);
	wcsncat_s(NewPath, NewPathSize + 1, Path2, Path2Size);
	NewPath[NewPathSize] = L'\0';

	return NewPath;
}

static HRESULT enum_recursively(LPWSTR Source, LPWSTR Destination, file_callback begin_cb, file_callback cb, file_callback end_cb)
{
	WIN32_FIND_DATAW FindData = { 0 };
	HANDLE hFind;
	HRESULT result = S_OK;
	LPWSTR Pattern = NULL;
	size_t PatternSize;

	if (begin_cb) {
		result = begin_cb(Source, Destination);
		if (FAILED(result))
			return result;
	}

	hFind = FindFirstFileW(Source, &FindData);
	if (INVALID_HANDLE_VALUE == hFind) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		FindClose(hFind);
		PatternSize = wcslen(Source) + 2;
		Pattern = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (PatternSize + 1) * sizeof(WCHAR));
		if (!Pattern) {
			result = E_OUTOFMEMORY;
			goto cleanup1;
		}
		wcsncpy_s(Pattern, PatternSize + 1, Source, wcslen(Source));
		wcsncat_s(Pattern, PatternSize + 1, L"\\*", 2);
		
		hFind = FindFirstFileW(Pattern, &FindData);
		if (INVALID_HANDLE_VALUE == hFind) {
			result = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup4;
		}
	}
	else {
		result = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
		goto cleanup1;
	}

	do {
		LPWSTR NewSource, NewDestination = NULL;

		if (FindData.cFileName[0] == '.' && !FindData.cFileName[1])
			continue;
		if (FindData.cFileName[0] == '.' && FindData.cFileName[1] == '.' && !FindData.cFileName[2])
			continue;

		NewSource = append_alloc(Source, FindData.cFileName);
		if (!NewSource) {
			result = E_OUTOFMEMORY;
			goto cleanup4;
		}

		if (Destination) {
			NewDestination = append_alloc(Destination, FindData.cFileName);
			if (!NewDestination) {
				result = E_OUTOFMEMORY;
				goto cleanup2;
			}
		}

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			result = enum_recursively(NewSource, NewDestination, begin_cb, cb, end_cb);
		}
		else {
			result = cb(NewSource, NewDestination);
		}

	cleanup3:
		if (NewDestination) HeapFree(GetProcessHeap(), 0, NewDestination);
	cleanup2:
		HeapFree(GetProcessHeap(), 0, NewSource);
	}
	while (SUCCEEDED(result) && FindNextFileW(hFind, &FindData));

	if (FAILED(result) && HRESULT_CODE(result) == ERROR_NO_MORE_FILES) {
		result = S_OK;
	}

	if (SUCCEEDED(result) && end_cb) {
		result = end_cb(Source, Destination);
	}

cleanup4:
	if (Pattern)
		HeapFree(GetProcessHeap(), 0, Pattern);
cleanup1:
	FindClose(hFind);
	return result;
}