#include "operations.h"
#include "utils.h"
#include "tests.h"

#include <windows.h>
#include <string.h>
#include "sds.h"
#include "cwalk.h"

HRESULT OpRenameFile(char *FilePath, char *NewName)
{
	sds NewPath;
	size_t BufferSize;
	HRESULT result = S_OK;

	NewPath = sdsempty();
	BufferSize = cwk_path_change_basename(FilePath, NewName, NewPath, 0);
	NewPath = sdsgrowzero(NewPath, BufferSize+1);
	if (!NewPath) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	cwk_path_change_basename(FilePath, NewName, NewPath, BufferSize);
	sdsupdatelen(NewPath);

	if (!MoveFile(FilePath, NewPath))
		result = HRESULT_FROM_WIN32(GetLastError());

	sdsfree(NewPath);
exit_fn:
	return result;
}