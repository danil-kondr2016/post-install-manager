#include "WPIW.h"
#include "execute.h"
#include "sds.h"
#include "utils.h"
#include "command.h"

#include <assert.h>

struct CommandInterpreters
{
	LPWSTR cmd;
	LPWSTR wscript;
	LPWSTR powershell;
};

static LPWSTR ExpandEnvironmentVariablesWAlloc(LPWSTR Source)
{
	LPWSTR szwExpanded = NULL;
	DWORD szwExpandedSize = 0;
	LPWSTR szwSource;

	szwExpandedSize = ExpandEnvironmentStringsW(Source, szwExpanded, 0);
	szwExpanded = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (szwExpandedSize + 1) * sizeof(WCHAR));
	if (!szwExpanded) {
		return NULL;
	}
	ExpandEnvironmentStringsW(Source, szwExpanded, szwExpandedSize);

	return szwExpanded;
}

static sds ExpandEnvironmentVariablesSds(const char *Source)
{
	LPWSTR szwExpanded = NULL;
	DWORD szwExpandedSize = 0;
	LPWSTR szwSource;
	sds result = NULL;

	szwSource = UTF8ToWideCharAlloc(Source);
	if (!szwSource)
		return NULL;

	szwExpanded = ExpandEnvironmentVariablesWAlloc(szwSource);
	if (!szwExpanded) {
		goto error_cleanup1;
	}
	HeapFree(GetProcessHeap(), 0, szwSource);
	szwSource = NULL;

	result = WideCharToSdsAlloc(szwExpanded);
	HeapFree(GetProcessHeap(), 0, szwExpanded);

	return result;
error_cleanup1:
	if (szwSource) HeapFree(GetProcessHeap(), 0, szwSource);
	return NULL;
}

static sds GetEnvironmentVariableWAlloc(LPCWSTR szwName)
{
	LPWSTR szwValue = NULL;
	DWORD szwValueSize = 0;

	szwValueSize = GetEnvironmentVariableW(szwName, szwValue, 0);
	szwValue = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (szwValueSize + 1) * sizeof(WCHAR));
	if (!szwValue) {
		return NULL;
	}
	GetEnvironmentVariableW(szwName, szwValue, szwValueSize);
	
	return szwValue;
}

static LPWSTR GetCurrentDirectoryWAlloc()
{
	DWORD szwCwdSize;
	LPWSTR szwCwd;

	szwCwdSize = GetCurrentDirectoryW(0, NULL);
	szwCwd = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (szwCwdSize + 1) * sizeof(WCHAR));
	if (!szwCwd)
		return NULL;
	GetCurrentDirectoryW(szwCwdSize, szwCwd);

	return szwCwd;
}

static int init_executables(struct CommandInterpreters *strings);

static HRESULT find_executable(LPWSTR Name, LPWSTR Path, LPWSTR CommandLine,
	_Out_ LPWSTR *pApplication,
	_Out_ LPWSTR *pCommandLine);

static HRESULT RunW(LPWSTR Application, LPWSTR CommandLine, DWORD *Status);

HRESULT Run(char *Application, char *CommandLine, DWORD *Status)
{
	LPWSTR szwApplication, szwCommandLine;
	HRESULT result = S_OK;

	szwApplication = UTF8ToWideCharAlloc(Application);
	if (!szwApplication) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	szwCommandLine = UTF8ToWideCharAlloc(CommandLine);
	if (!szwCommandLine) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}

	result = RunW(szwApplication, szwCommandLine, Status);

cleanup2:
	HeapAlloc(GetProcessHeap(), 0, szwCommandLine);
cleanup1:
	HeapAlloc(GetProcessHeap(), 0, szwApplication);
exit_fn:
	return result;
}

static HRESULT RunW(LPWSTR Application, LPWSTR CommandLine, DWORD *Status)
{
	LPWSTR szwCwd;
	LPWSTR szwPath, szwCurrent, pSemicolon;
	LPWSTR szwApplication, szwCommandLine;
	HRESULT result = S_OK;

	szwCwd = GetCurrentDirectoryWAlloc();
	if (!szwCwd) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}
	szwPath = GetEnvironmentVariableWAlloc(L"PATH");
	if (!szwPath) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}

	result = find_executable(Application, szwCwd, CommandLine, &szwApplication, &szwCommandLine);
	if (FAILED(result)) {
		szwCurrent = szwPath;
		do {
			pSemicolon = wcschr(szwCurrent, L';');
			if (pSemicolon) {
				*pSemicolon = '\0';
			}

			result = find_executable(Application, szwCurrent, CommandLine, &szwApplication, &szwCommandLine);
			if (SUCCEEDED(result)) {
				break;
			}

			szwCurrent = pSemicolon + 1;
		}
		while (pSemicolon);
	}
	if (SUCCEEDED(result)) {
		STARTUPINFOW StartupInfo = { 0 };
		PROCESS_INFORMATION ProcessInfo = { 0 };
		BOOL CreationResult;

		StartupInfo.cb = sizeof(StartupInfo);
		CreationResult = CreateProcessW(szwApplication, szwCommandLine, NULL, NULL, FALSE,
			NORMAL_PRIORITY_CLASS,
			NULL, NULL, &StartupInfo, &ProcessInfo);

		if (!CreationResult) {
			result = HRESULT_FROM_WIN32(GetLastError());
			*Status = 1;
		}
		else {
			WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
			if (!GetExitCodeProcess(ProcessInfo.hProcess, Status)) {
				result = HRESULT_FROM_WIN32(GetLastError());
				*Status = 1;
			}
			CloseHandle(ProcessInfo.hProcess);
			CloseHandle(ProcessInfo.hThread);
		}
	}
	else {
		*Status = 1;
	}

cleanup2:
	HeapFree(GetProcessHeap(), 0, szwPath);
cleanup1:
	HeapFree(GetProcessHeap(), 0, szwCwd);
exit_fn:
	return result;
}

static int init_executables(struct CommandInterpreters *strings)
{
	static const WCHAR cmd_orig[] = L"%SystemRoot%\\System32\\cmd.exe";
	static const WCHAR wscript_orig[] = L"%SystemRoot%\\System32\\wscript.exe";
	static const WCHAR powershell_orig[] = L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
	int result = 0;

	assert(strings);
	strings->cmd = ExpandEnvironmentVariablesWAlloc(cmd_orig);
	if (!strings->cmd)
		goto error0;

	strings->wscript = ExpandEnvironmentVariablesWAlloc(wscript_orig);
	if (!strings->wscript)
		goto error1;

	strings->powershell = ExpandEnvironmentVariablesWAlloc(powershell_orig);
	if (!strings->powershell)
		goto error2;

success:
	return 1;
error3:
	HeapFree(GetProcessHeap(), 0, strings->powershell);
	strings->powershell = NULL;
error2:
	HeapFree(GetProcessHeap(), 0, strings->wscript);
	strings->wscript = NULL;
error1:
	HeapFree(GetProcessHeap(), 0, strings->cmd);
	strings->cmd = NULL;
error0:
	return 0;
}

static LPWSTR build_command_line(LPWSTR Preamble, LPWSTR ScriptName, LPWSTR CommandLine)
{
	LPWSTR result = NULL;
	DWORD resultSize;

	resultSize = wcslen(Preamble) + wcslen(ScriptName) + wcslen(CommandLine) + 2;
	resultSize += 8 - (resultSize & 8);
	result = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (resultSize + 1) * sizeof(WCHAR));
	if (!result)
		return NULL;
	wcscpy_s(result, resultSize + 1, Preamble);
	wcscat_s(result, resultSize + 1, L" ");
	wcscat_s(result, resultSize + 1, ScriptName);
	wcscat_s(result, resultSize + 1, L" ");
	wcscat_s(result, resultSize + 1, CommandLine);

	return result;
}

static LPWSTR duplicate_wide_string(LPWSTR Source)
{
	LPWSTR result;
	DWORD resultSize;

	if (!Source)
		return NULL;

	resultSize = wcslen(Source);
	result = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (resultSize + 1) * sizeof(WCHAR));
	if (!result)
		return NULL;
	wcscpy_s(result, resultSize + 1, Source);

	return result;
}

static int test_file(LPWSTR Path)
{
	HANDLE hFind;
	WIN32_FIND_DATAW FindData = { 0 };

	hFind = FindFirstFileW(Path, &FindData);
	if (hFind == INVALID_HANDLE_VALUE || FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return 0;
	}
	FindClose(hFind);

	return 1;
}

static HRESULT find_executable(LPWSTR Name, LPWSTR Path, LPWSTR CommandLine, 
	_Out_ LPWSTR *pApplication, 
	_Out_ LPWSTR *pCommandLine)
{
	static struct CommandInterpreters interpreters;
	static struct
	{
		LPWSTR Extension;
		LPWSTR *Interpreter;
		LPWSTR Preamble;
	} extensionData[] = {
		{L".EXE", NULL, L""},
		{L".BAT", &(interpreters.cmd),        L"/q /c"},
		{L".CMD", &(interpreters.cmd),        L"/q /c"},
		{L".VBS", &(interpreters.wscript),    L""},
		{L".VBE", &(interpreters.wscript),    L""},
		{L".JS",  &(interpreters.wscript),    L""},
		{L".JSE", &(interpreters.wscript),    L""},
		{L".WSF", &(interpreters.wscript),    L""},
		{L".WSH", &(interpreters.wscript),    L""},
		{L".PS1", &(interpreters.powershell), L"-ExecutionPolicy Bypass -File"},
		{NULL, NULL, NULL}
	};
	HRESULT result = 0;
	LPWSTR szwFullScriptPath;
	DWORD szwFullScriptPathSize;
	LPWSTR extension;
	LPWSTR szwPreamble = NULL, szwInterpreter = NULL;

	if (!interpreters.cmd)
		if (!init_executables(&interpreters))
			return E_OUTOFMEMORY;
	if (!interpreters.cmd || !interpreters.wscript || !interpreters.powershell)
		return E_OUTOFMEMORY;

	szwFullScriptPathSize = wcslen(Path) + wcslen(Name) + 1 + 4; // Path+\+Name+.EXT up to 3 letters
	szwFullScriptPath = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (szwFullScriptPathSize + 1) * sizeof(WCHAR));
	if (!szwFullScriptPath)
		return E_OUTOFMEMORY;
	wcscpy_s(szwFullScriptPath, szwFullScriptPathSize + 1, Path);
	wcscat_s(szwFullScriptPath, szwFullScriptPathSize + 1, L"\\");
	wcscat_s(szwFullScriptPath, szwFullScriptPathSize + 1, Name);

	if ((extension = wcsrchr(szwFullScriptPath, '.')) != NULL) {
		int i;

		if (!test_file(szwFullScriptPath)) {
			HeapFree(GetProcessHeap(), 0, szwFullScriptPath);
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}

		for (i = 0; extensionData[i].Extension; i++) {
			if (!lstrcmpiW(extension, extensionData[i].Extension)) {
				if (extensionData[i].Interpreter)
					szwInterpreter = *extensionData[i].Interpreter;
				else
					szwInterpreter = NULL;
				szwPreamble = extensionData[i].Preamble;
				break;
			}
		}

		if (!extensionData[i].Extension) {
			result = WPIW_E_EXECUTABLE_NOT_FOUND;
			goto cleanup1;
		}
	}
	else {
		DWORD szwFullScriptPathLength;
		LPWSTR szwFullScriptPathEnd;
		int i;

		szwFullScriptPathLength = wcslen(szwFullScriptPath);
		szwFullScriptPathEnd = szwFullScriptPath + szwFullScriptPathLength;

		result = WPIW_E_EXECUTABLE_NOT_FOUND;

		for (i = 0; extensionData[i].Extension; i++) {
			*szwFullScriptPathEnd = L'\0';
			wcscat_s(szwFullScriptPath, szwFullScriptPathSize + 1, extensionData[i].Extension);

			if (test_file(szwFullScriptPath)) {
				if (extensionData[i].Interpreter)
					szwInterpreter = *extensionData[i].Interpreter;
				else
					szwInterpreter = NULL;
				szwPreamble = extensionData[i].Preamble;
				result = S_OK;
				break;
			}
		}
	}

	if (SUCCEEDED(result)) {
		if (szwInterpreter) {
			*pApplication = duplicate_wide_string(szwInterpreter);
			if (!*pApplication) {
				result = E_OUTOFMEMORY;
				goto cleanup1;
			}

			*pCommandLine = build_command_line(szwPreamble, szwFullScriptPath, CommandLine);
			if (!*pCommandLine) {
				HeapFree(GetProcessHeap(), 0, *pApplication);
				result = E_OUTOFMEMORY;
				goto cleanup1;
			}
		}
		else {
			*pApplication = duplicate_wide_string(szwFullScriptPath);
			if (!*pApplication) {
				result = E_OUTOFMEMORY;
				goto cleanup1;
			}

			*pCommandLine = duplicate_wide_string(CommandLine);
			if (!*pCommandLine) {
				HeapFree(GetProcessHeap(), 0, *pApplication);
				result = E_OUTOFMEMORY;
				goto cleanup1;
			}
		}
	}
	else {
		*pApplication = NULL;
		*pCommandLine = NULL;
	}

cleanup1:
	HeapFree(GetProcessHeap(), 0, szwFullScriptPath);
	return result;
}