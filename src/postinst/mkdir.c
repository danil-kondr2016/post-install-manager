#include "fileops.h"
#include "tests.h"
#include "errors.h"

#include <windows.h>
#include <cwalk.h>

uint32_t op_mkdir(char *path, struct arena scratch)
{
	uint32_t result = 0;
	char *full_path, *path_segment, *path_segment_end;
	char *Context, *Next;
	struct cwk_segment segment = { 0 };
	struct arena old_scratch = scratch;

	if (!cwk_path_get_first_segment(path, &segment))
		return 1;

	full_path = NULL;
	do {
		path_segment = arena_new(&scratch, char, segment.size + 1);
		memcpy(path_segment, segment.begin, segment.size);
		if (!full_path) full_path = path_segment;
		path_segment_end = path_segment + segment.size;

		old_scratch = scratch;
		wchar_t *full_pathW = u8_to_u16(full_path, &scratch);
		if (!CreateDirectoryW(full_pathW, NULL)) {
			DWORD error = GetLastError();
			if (error != ERROR_ALREADY_EXISTS) {
				result = NTSTATUS_FROM_WIN32(GetLastError());
				break;
			}
		}
		scratch = old_scratch;

		if (cwk_path_get_next_segment(&segment))
			*path_segment_end = '\\';
		else
			break;
	}
	while (1);

exit_fn:
	return result;
}
