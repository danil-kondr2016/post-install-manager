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
	LPWSTR szwOldPath, szwNewPath;

	NewPath = sdsempty();
	BufferSize = cwk_path_change_basename(FilePath, NewName, NewPath, 0);
	NewPath = sdsgrowzero(NewPath, BufferSize+1);
	if (!NewPath) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	cwk_path_change_basename(FilePath, NewName, NewPath, BufferSize);
	sdsupdatelen(NewPath);

	szwOldPath = UTF8ToWideCharAlloc(FilePath);
	if (!szwOldPath) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}

	szwNewPath = UTF8ToWideCharAlloc(NewPath);
	if (!szwNewPath) {
		result = E_OUTOFMEMORY;
		goto cleanup2;
	}

	MoveFileW(szwOldPath, szwNewPath);
	result = HRESULT_FROM_WIN32(GetLastError());

cleanup3:
	HeapFree(GetProcessHeap(), 0, szwNewPath);
cleanup2:
	HeapFree(GetProcessHeap(), 0, szwOldPath);
cleanup1:
	sdsfree(NewPath);
exit_fn:
	return result;
}