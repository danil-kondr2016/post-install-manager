#include "stdafx.h"
#include "operations.h"
#include "tests.h"

typedef HRESULT (*file_callback)(LPSTR arg1, LPSTR arg2);

static HRESULT copy_single(LPSTR Source, LPSTR Destination)
{
	if (!CopyFile(Source, Destination, FALSE))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT delete_single(LPSTR File, LPSTR Unused)
{
	(void)Unused;
	if (!DeleteFile(File))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT create_destination(LPSTR Unused, LPSTR Destination)
{
	(void)Unused;
	if (!CreateDirectory(Destination, NULL))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT delete_folder(LPSTR Source, LPSTR Unused)
{
	(void)Unused;
	if (!RemoveDirectory(Source))
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

static HRESULT enum_recursively(LPSTR Source, LPSTR Destination, file_callback begin_cb, file_callback cb, file_callback end_cb);

HRESULT OpCopyDirectory(char *Source, char *Destination)
{
	HRESULT result = S_OK;
	sds NewDestination = NULL;
	char *BaseName;

	cwk_path_get_basename(Source, &BaseName, NULL);
	if (TestIsDirectory(Destination)) {
		NewDestination = sdsempty();
		size_t NewDestinationSize;

		NewDestinationSize = cwk_path_join(Destination, BaseName, NewDestination, 0);
		NewDestination = sdsgrowzero(NewDestination, NewDestinationSize + 1);
		cwk_path_join(Destination, BaseName, NewDestination, NewDestinationSize);

		Destination = NewDestination;
	}

	result = enum_recursively(Source, Destination, create_destination, copy_single, NULL);

	sdsfree(NewDestination);
exit_fn:
	return result;
}

HRESULT OpRemoveRecurse(char* Source)
{
	return enum_recursively(Source, NULL, NULL, delete_single, delete_folder);
}

static sds append_alloc(char *Path1, char *Path2)
{
	sds NewPath;

	NewPath = sdsnew(Path1);
	NewPath = sdscat(NewPath, "\\");
	NewPath = sdscat(NewPath, Path2);

	return NewPath;
}

static HRESULT enum_recursively(LPSTR Source, LPSTR Destination, file_callback begin_cb, file_callback cb, file_callback end_cb)
{
	WIN32_FIND_DATA FindData = { 0 };
	HANDLE hFind;
	HRESULT result = S_OK;
	sds Pattern = NULL;

	if (begin_cb) {
		result = begin_cb(Source, Destination);
		if (FAILED(result))
			return result;
	}

	hFind = FindFirstFile(Source, &FindData);
	if (INVALID_HANDLE_VALUE == hFind) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		FindClose(hFind);
		Pattern = sdscat(sdsnew(Source), "\\*");
		
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
		sds NewSource, NewDestination = NULL;

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
		if (NewDestination) sdsfree(NewDestination);
	cleanup2:
		sdsfree(NewSource);
	}
	while (SUCCEEDED(result) && FindNextFile(hFind, &FindData));

	if (FAILED(result) && HRESULT_CODE(result) == ERROR_NO_MORE_FILES) {
		result = S_OK;
	}

	if (SUCCEEDED(result) && end_cb) {
		result = end_cb(Source, Destination);
	}

cleanup4:
	if (Pattern)
		sdsfree(Pattern);
cleanup1:
	FindClose(hFind);
	return result;
}