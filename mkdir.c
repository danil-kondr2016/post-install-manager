#include "stdafx.h"

#include "operations.h"
#include "tests.h"
#include "utils.h"

HRESULT OpMakeDirectory(char *Directory)
{
	HRESULT result = S_OK;
	sds FullPath = NULL, FullPathNew;
	char *Context, *Next;
	struct cwk_segment segment = { 0 };

	if (!cwk_path_get_first_segment(Directory, &segment))
		return S_FALSE;

	FullPath = sdsempty();
	if (!FullPath)
		return E_OUTOFMEMORY;

	do {
		FullPathNew = sdscatlen(FullPath, segment.begin, segment.size);
		if (!FullPathNew) {
			result = E_OUTOFMEMORY;
			goto cleanup1;
		}
		FullPath = FullPathNew;

		if (!CreateDirectory(FullPath, NULL)) {
			result = HRESULT_FROM_WIN32(GetLastError());
			break;
		}

		if (cwk_path_get_next_segment(&segment)) {
			FullPathNew = sdscatlen(FullPath, "\\", 1);
			if (!FullPathNew) {
				result = E_OUTOFMEMORY;
				goto cleanup1;
			}
			FullPath = FullPathNew;
		}
		else {
			break;
		}
	}
	while (1);

cleanup1:
	sdsfree(FullPath);
exit_fn:
	return result;
}