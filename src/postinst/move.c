#include "fileops.h"
#include "tests.h"
#include "errors.h"

#include <windows.h>
#include <cwalk.h>

uint32_t op_move(char *source, char *dest, struct arena scratch)
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
	if (!MoveFileExW(sourceW, destW, MOVEFILE_COPY_ALLOWED))
		result = NTSTATUS_FROM_WIN32(GetLastError());

	return result;
}
