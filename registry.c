#include "stdafx.h"

#include "operations.h"

HRESULT OpRegImport(char *RegistryFile)
{
	HRESULT result = S_OK;
	DWORD status;
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	char RegPath[MAX_PATH] = { 0 };
	sds CommandLine = NULL;

	// Should not be truncated, on Windows versions starting with Vista
	// it always should be "C:\Windows\System32\reg.exe" and should not
	// exceed 260 bytes in UTF-8 format.
	ExpandEnvironmentStrings("%SystemRoot%\\System32\\reg.exe", RegPath, MAX_PATH);
	CommandLine = sdscat(sdsnew("import "), RegistryFile);

	si.cb = sizeof(si);
	if (!CreateProcess(RegPath, CommandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		result = HRESULT_FROM_WIN32(GetLastError());
	}
	else {
		if (!pi.hProcess) {
			result = HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
		}
		else {
			WaitForSingleObject(pi.hProcess, INFINITE);
			if (!GetExitCodeProcess(pi.hProcess, &status)) {
				result = HRESULT_FROM_WIN32(GetLastError());
			}
			if (status == 0)
				result = S_OK;
			else
				result = HRESULT_FROM_WIN32(ERROR_NOT_REGISTRY_FILE);
		}
	}

	sdsfree(CommandLine);
	return result;
}