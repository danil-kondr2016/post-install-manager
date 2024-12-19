#define _CRT_SECURE_NO_WARNINGS

#include "fileops.h"
#include "tests.h"
#include "errors.h"

#include <string.h>
#include <windows.h>
#include <cwalk.h>

typedef uint32_t (*file_callback)(wchar_t *arg1, wchar_t *arg2, struct arena scratch);

static uint32_t copy_single(wchar_t *source, wchar_t *dest, struct arena scratch);
static uint32_t delete_single(wchar_t *File, wchar_t *unused, struct arena scratch);
static uint32_t create_destination(wchar_t *unused, wchar_t *dest, struct arena scratch);
static uint32_t delete_folder(wchar_t *source, wchar_t *unused, struct arena scratch);

static uint32_t enum_recursively(wchar_t *source, wchar_t *dest, 
		struct arena scratch, 
		file_callback begin_cb, 
		file_callback cb, 
		file_callback end_cb);

uint32_t op_copy_dir(char *source, char *dest, struct arena scratch)
{
	uint32_t result = 0;
	const char *base_name;
	wchar_t *sourceW, *destW;

	cwk_path_get_basename(source, &base_name, NULL);
	if (test_is_directory(dest, scratch)) {
		size_t new_dest_size;
		char *new_dest;

		new_dest_size = cwk_path_join(dest, base_name, NULL, 0);
		new_dest = arena_new(&scratch, char, new_dest_size + 1);
		cwk_path_join(dest, base_name, new_dest, new_dest_size + 1);
		
		dest = new_dest;
	}

	sourceW = u8_to_u16(source, &scratch);
	destW = u8_to_u16(dest, &scratch);
	result = enum_recursively(sourceW, destW, scratch, create_destination, copy_single, NULL);

	return result;
}

uint32_t op_remove_dir(char* source, struct arena scratch)
{
	wchar_t *sourceW;
	sourceW = u8_to_u16(source, &scratch);
	return enum_recursively(sourceW, NULL, scratch, NULL, delete_single, delete_folder);
}

static wchar_t *append_alloc(wchar_t *Path1, wchar_t *Path2, struct arena *scratch)
{
	wchar_t *NewPath;
	size_t Path1_size, Path2_size;

	Path1_size = wcslen(Path1);
	Path2_size = wcslen(Path2);
	NewPath = arena_new(scratch, wchar_t, Path1_size + Path2_size + 2);

	wcscpy(NewPath, Path1);
	wcscat(NewPath, L"\\");
	wcscat(NewPath, Path2);

	return NewPath;
}

static uint32_t enum_recursively(wchar_t *source, wchar_t *dest, 
		struct arena scratch, 
		file_callback begin_cb, 
		file_callback cb, 
		file_callback end_cb)
{
	WIN32_FIND_DATAW FindData = { 0 };
	HANDLE hFind;
	uint32_t result = 0;

	if (begin_cb) {
		result = begin_cb(source, dest, scratch);
		if (FAILED(result))
			return result;
	}

	hFind = FindFirstFileW(source, &FindData);
	if (INVALID_HANDLE_VALUE == hFind) {
		return NTSTATUS_FROM_WIN32(GetLastError());
	}

	if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		FindClose(hFind);
		wchar_t *Pattern = append_alloc(source, L"*", &scratch);
		
		hFind = FindFirstFileW(Pattern, &FindData);
		if (INVALID_HANDLE_VALUE == hFind) {
			result = NTSTATUS_FROM_WIN32(GetLastError());
			return result;
		}
	}
	else {
		result = NTSTATUS_FROM_WIN32(ERROR_PATH_NOT_FOUND);
		goto cleanup1;
	}

	do {
		wchar_t *new_source, *new_dest = NULL;

		if (FindData.cFileName[0] == '.' && !FindData.cFileName[1])
			continue;
		if (FindData.cFileName[0] == '.' && FindData.cFileName[1] == '.' && !FindData.cFileName[2])
			continue;

		new_source = append_alloc(source, FindData.cFileName, &scratch);
		if (dest) {
			new_dest = append_alloc(dest, FindData.cFileName, &scratch);
		}

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			result = enum_recursively(source, dest, scratch, begin_cb, cb, end_cb);
		}
		else {
			result = cb(new_source, new_dest, scratch);
		}

		if (FAILED(result)) // ignore any errors
			result = 1;
	}
	while (SUCCEEDED(result) && FindNextFileW(hFind, &FindData));

	if (FAILED(result) && (result & 0xFFFF) == ERROR_NO_MORE_FILES) {
		result = 0;
	}

	if (SUCCEEDED(result) && end_cb) {
		result = end_cb(source, dest, scratch);
	}

cleanup1:
	FindClose(hFind);
	return result;
}

static uint32_t copy_single(wchar_t *source, wchar_t *dest, struct arena scratch)
{
	if (!CopyFileW(source, dest, FALSE))
		return NTSTATUS_FROM_WIN32(GetLastError());
	return 0;
}

static uint32_t delete_single(wchar_t *file, wchar_t *unused, struct arena scratch)
{
	(void)unused;
	if (!DeleteFileW(file))
		return NTSTATUS_FROM_WIN32(GetLastError());
	return 0;
}

static uint32_t create_destination(wchar_t *unused, wchar_t *dest, struct arena scratch)
{
	(void)unused;
	if (!CreateDirectoryW(dest, NULL))
		return NTSTATUS_FROM_WIN32(GetLastError());
	return 0;
}

static uint32_t delete_folder(wchar_t *source, wchar_t *unused, struct arena scratch)
{
	(void)unused;
	if (!RemoveDirectoryW(source))
		return NTSTATUS_FROM_WIN32(GetLastError());
	return 0;
}
