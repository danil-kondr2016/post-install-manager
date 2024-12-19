#include "fileops.h"
#include "tests.h"
#include "errors.h"

#include <windows.h>
#include <cwalk.h>

uint32_t op_rename(char *path, char *new_name, struct arena scratch)
{
	char *new_path;
	size_t new_path_size;
	uint32_t result = S_OK;
	wchar_t *pathW, *new_pathW;

	new_path_size = cwk_path_change_basename(path, new_name, new_path, 0);
	new_path = arena_new(&scratch, char, new_path_size + 1);
	cwk_path_change_basename(path, new_name, new_path, new_path_size);

	pathW = u8_to_u16(path, &scratch);
	new_pathW = u8_to_u16(new_path, &scratch);

	if (!MoveFileW(pathW, new_pathW))
		result = NTSTATUS_FROM_WIN32(GetLastError());

	return result;
}
