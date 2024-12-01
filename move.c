#include "stdafx.h"
#include "operations.h"
#include "tests.h"

HRESULT OpMoveFile(char *Source, char *Destination)
{
	HRESULT result = S_OK;
	char *BaseName;
	sds NewDestination = NULL;

	cwk_path_get_basename(Source, &BaseName, NULL);
	if (TestIsDirectory(Destination)) {
		NewDestination = sdsempty();
		size_t NewDestinationSize;

		NewDestinationSize = cwk_path_join(Destination, BaseName, NewDestination, 0);
		NewDestination = sdsgrowzero(NewDestination, NewDestinationSize + 1);
		cwk_path_join(Destination, BaseName, NewDestination, NewDestinationSize + 1);
		sdsupdatelen(NewDestination);

		Destination = NewDestination;
	}

	if (!MoveFileEx(Source, Destination, MOVEFILE_COPY_ALLOWED))
		result = HRESULT_FROM_WIN32(GetLastError());
	sdsfree(NewDestination);

	return result;
}