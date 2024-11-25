#include "operations.h"
#include "utils.h"
#include "tests.h"

#include <windows.h>
#include <string.h>
#include "sds.h"
#include "cwalk.h"

HRESULT OpMoveFile(char *Source, char *Destination)
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

		NewDestinationSize = cwk_path_get_absolute(Destination, BaseName, NewDestination, 0);
		sdsgrowzero(NewDestination, NewDestinationSize + 1);
		cwk_path_get_absolute(Destination, BaseName, NewDestination, NewDestinationSize);

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

	MoveFileW(szwSource, szwDestination);
	result = HRESULT_FROM_WIN32(GetLastError());

cleanup2:
	HeapFree(GetProcessHeap(), 0, szwDestination);
cleanup1:
	HeapFree(GetProcessHeap(), 0, szwSource);
exit_fn:
	return result;
}