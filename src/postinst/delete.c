#include "fileops.h"
#include "errors.h"

#include <windows.h>

uint32_t op_remove_file(char *file, struct arena scratch)
{
	uint32_t result = S_OK;
	wchar_t *fileW;

	fileW = u8_to_u16(file, &scratch);
	if (!DeleteFileW(fileW))
		result = NTSTATUS_FROM_WIN32(GetLastError());

	return result;
}
