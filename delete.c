#include "stdafx.h"
#include "operations.h"
#include "tests.h"

HRESULT OpRemoveFile(char* File)
{
	HRESULT result = S_OK;

	if (!DeleteFile(File))
		result = HRESULT_FROM_WIN32(GetLastError());

	return result;
}

HRESULT OpRemoveDirectory(char *Directory)
{
	HRESULT result = S_OK;

	if (!RemoveDirectory(Directory))
		result = HRESULT_FROM_WIN32(GetLastError());

	return result;
}