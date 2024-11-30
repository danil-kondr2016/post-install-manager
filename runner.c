#include "stdafx.h"
#include "die.h"
#include "runner.h"
#include "utils.h"

struct
{
	CHAR Extension[8];
	CHAR Preamble[64];
	enum InterpreterType Interpreter;
} ExtensionData[] = {
	{".EXE", "",                              INTERPRETER_NULL},
	{".BAT", "/q /c",                         INTERPRETER_CMD},
	{".CMD", "/q /c",                         INTERPRETER_CMD},
	{".VBS", "",                              INTERPRETER_WSCRIPT},
	{".VBE", "",                              INTERPRETER_WSCRIPT},
	{".JS",  "",                              INTERPRETER_WSCRIPT},
	{".JSE", "",                              INTERPRETER_WSCRIPT},
	{".WSF", "",                              INTERPRETER_WSCRIPT},
	{".WSH", "",                              INTERPRETER_WSCRIPT},
	{".PS1", "-ExecutionPolicy Bypass -File", INTERPRETER_POWERSHELL},
	{"",     "",                              INTERPRETER_NULL},
};

struct PathEntry
{
	LPSTR Path;
	struct PathEntry *Next;
};

static LPVOID runner_alloc(PEXERUNNER Runner, DWORD Size, DWORD Alignment, DWORD Count);
static HRESULT init_pool(PEXERUNNER Runner);
static HRESULT free_pool(PEXERUNNER Runner);
static HRESULT load_interpreters(PEXERUNNER Runner);
static HRESULT load_path(PEXERUNNER Runner);

static inline void set_errorlevel(PEXERUNNER Runner, DWORD ErrorLevel, DWORD *Status)
{
	*Status = Runner->ErrorLevel = ErrorLevel;
}

HRESULT InitExecutableRunner(PEXERUNNER Runner)
{
	HRESULT Result = S_OK;

	if (Runner->PoolStart)
		return S_FALSE;

	memset(Runner, 0, sizeof(EXERUNNER));
	Result = init_pool(Runner);
	if (FAILED(Result)) return Result;

	Result = load_interpreters(Runner);
	if (FAILED(Result)) return Result;
	
	Result = load_path(Runner);
	return Result;
}

HRESULT FreeExecutableRunner(PEXERUNNER Runner)
{
	HRESULT result;
	
	result = free_pool(Runner);
	if (FAILED(result))
		return result;

	memset(Runner, 0, sizeof(EXERUNNER));

	return S_OK;
}

static LPVOID runner_alloc(PEXERUNNER Runner, DWORD Size, DWORD Alignment, DWORD Count)
{
	ptrdiff_t padding, available;
	LPVOID Result;

	padding = -(ptrdiff_t)Runner->PoolPtr & (Alignment - 1);
	available = Runner->PoolEnd - Runner->PoolPtr - padding;
	if (available <= 0 || Count > available / Size) {
		return NULL;
	}

	Result = Runner->PoolPtr + padding;
	Runner->PoolPtr += padding + (size_t)Size * Count;
	return memset(Result, 0, (size_t)Size * Count);
}

#define RUNNER_POOL_SIZE (1048576L)

static HRESULT init_pool(PEXERUNNER Runner)
{
	assert(Runner);

	Runner->PoolStart = VirtualAlloc(NULL, RUNNER_POOL_SIZE,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!Runner->PoolStart)
		FatalError(HRESULT_FROM_WIN32(GetLastError()));

	Runner->PoolPtr = Runner->PoolStart;
	Runner->PoolEnd = Runner->PoolStart + RUNNER_POOL_SIZE;

	return S_OK;
}

static HRESULT free_pool(PEXERUNNER Runner)
{
	if (!VirtualFree(Runner->PoolStart, 0, MEM_RELEASE)) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	
	memset(Runner, 0, sizeof(EXERUNNER));
	return S_OK;
}

static LPSTR expand_vars_into_pool(PEXERUNNER Runner, LPSTR Source)
{
	LPSTR Expanded = NULL;
	DWORD ExpandedSize = 0;
	ExpandedSize = ExpandEnvironmentStrings(Source, Expanded, 0);
	Expanded = (LPSTR)runner_alloc(Runner, sizeof(CHAR), sizeof(CHAR), ExpandedSize + 1);
	if (!Expanded)
		return NULL;
	ExpandEnvironmentStrings(Source, Expanded, ExpandedSize);

	return Expanded;
}

static HRESULT load_interpreters(PEXERUNNER Runner)
{
	static const CHAR cmd_orig[] = "%SystemRoot%\\System32\\cmd.exe";
	static const CHAR wscript_orig[] = "%SystemRoot%\\System32\\wscript.exe";
	static const CHAR powershell_orig[] = "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";

	assert(Runner);

	Runner->Interpreters[INTERPRETER_NULL] = NULL;

	if (!(Runner->Interpreters[INTERPRETER_CMD] = expand_vars_into_pool(Runner, cmd_orig)))
		return E_OUTOFMEMORY;

	if (!(Runner->Interpreters[INTERPRETER_WSCRIPT] = expand_vars_into_pool(Runner, wscript_orig)))
		return E_OUTOFMEMORY;

	if (!(Runner->Interpreters[INTERPRETER_POWERSHELL] = expand_vars_into_pool(Runner, powershell_orig)))
		return E_OUTOFMEMORY;

	return S_OK;
}

static HRESULT load_path(PEXERUNNER Runner)
{
	HRESULT Result = S_OK;
	LPSTR Path, p, q;
	struct PathEntry *CurrentPathEntry = NULL, *NextPathEntry;
	SIZE_T SizeOfPath = 0;

	Path = expand_vars_into_pool(Runner, "%PATH%");
	if (!Path)
		return E_OUTOFMEMORY;
	SizeOfPath = strlen(Path);
	p = Path;
	q = Path;
	while ((p - Path) < SizeOfPath) {
		if (*p == ';') {
			*p = '\0';
			if (*(p + 1) != ';') {
				NextPathEntry = (struct PathEntry *)runner_alloc(Runner, sizeof(struct PathEntry), sizeof(void *), 1);
				if (!NextPathEntry) {
					return E_OUTOFMEMORY;
				}
				NextPathEntry->Path = q;

				if (!CurrentPathEntry) {
					CurrentPathEntry = NextPathEntry;
					Runner->PathEntries = CurrentPathEntry;
				}
				else {
					CurrentPathEntry->Next = NextPathEntry;
					CurrentPathEntry = CurrentPathEntry->Next;
				}
			}
			q = p + 1;
		}
		p++;
	}

	return S_OK;
}

static LPSTR build_command_line(PEXERUNNER Runner, LPSTR Preamble, LPSTR ScriptName, LPSTR CommandLine)
{
	LPSTR result = NULL;
	DWORD resultSize;

	resultSize = strlen(Preamble) + strlen(ScriptName) + strlen(CommandLine) + 2;
	result = (LPSTR)runner_alloc(Runner, sizeof(CHAR), sizeof(CHAR), resultSize + 1);
	if (!result)
		return E_OUTOFMEMORY;

	strcpy(result, Preamble);
	strcat(result, " ");
	strcat(result, ScriptName);
	strcat(result, " ");
	strcat(result, CommandLine);

	return result;
}

static LPSTR duplicate_string(PEXERUNNER Runner, LPSTR Source)
{
	LPSTR result;
	DWORD resultSize;

	if (!Source)
		return NULL;

	resultSize = strlen(Source);
	result = (LPSTR)runner_alloc(Runner, sizeof(CHAR), sizeof(CHAR), resultSize + 1);
	if (!result)
		return E_OUTOFMEMORY;

	strcpy(result, Source);

	return result;
}

static int test_file(LPSTR Path)
{
	HANDLE hFind;
	WIN32_FIND_DATA FindData = { 0 };

	hFind = FindFirstFile(Path, &FindData);
	if (hFind == INVALID_HANDLE_VALUE || FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return 0;
	}
	FindClose(hFind);

	return 1;
}

static HRESULT find_executable(PEXERUNNER Runner, LPSTR Name, LPSTR Path, LPSTR CommandLine,
	_Out_ LPSTR *pApplication,
	_Out_ LPSTR *pCommandLine)
{
	HRESULT result = 0;
	LPSTR FullScriptPath;
	DWORD FullScriptPathSize;
	LPSTR extension;
	LPSTR Preamble = NULL, Interpreter = NULL;

	FullScriptPathSize = strlen(Name) + 1;
	if (Path) FullScriptPathSize += strlen(Path);
	FullScriptPath = (LPSTR)runner_alloc(Runner, sizeof(CHAR), sizeof(CHAR), FullScriptPathSize + 1);
	if (!FullScriptPath)
		return E_OUTOFMEMORY;

	if (Path) {
		cwk_path_join(Path, Name, FullScriptPath, FullScriptPathSize + 1);
	} else {
		strcpy(FullScriptPath, Name);
	}

	if ((extension = strrchr(FullScriptPath, '.')) != NULL) {
		int i;

		if (!test_file(FullScriptPath)) {
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}

		for (i = 0; ExtensionData[i].Extension; i++) {
			if (!xstricmp(extension, ExtensionData[i].Extension)) {
				Interpreter = Runner->Interpreters[ExtensionData[i].Interpreter];
				Preamble = ExtensionData[i].Preamble;
				break;
			}
		}

		if (!*ExtensionData[i].Extension) {
			result = WPIW_E_EXECUTABLE_NOT_FOUND;
			goto cleanup;
		}
	}
	else {
		DWORD FullScriptPathLength;
		LPSTR FullScriptPathEnd;
		int i;

		FullScriptPathLength = strlen(FullScriptPath);
		FullScriptPathEnd = FullScriptPath + FullScriptPathLength;

		result = WPIW_E_EXECUTABLE_NOT_FOUND;
		extension = (LPSTR)runner_alloc(Runner, sizeof(CHAR), sizeof(CHAR), 4);
		if (!extension)
			return E_OUTOFMEMORY;
	
		for (i = 0; ExtensionData[i].Extension; i++) {
			*FullScriptPathEnd = L'\0';
			strcpy(FullScriptPathEnd, ExtensionData[i].Extension);
			if (test_file(FullScriptPath)) {
				Interpreter = Runner->Interpreters[ExtensionData[i].Interpreter];
				Preamble = ExtensionData[i].Preamble;
				result = S_OK;
				break;
			}
		}
	}

	if (SUCCEEDED(result)) {
		if (Interpreter) {
			*pApplication = duplicate_string(Runner, Interpreter);
			*pCommandLine = build_command_line(Runner, Preamble, FullScriptPath, CommandLine);
		}
		else {
			*pApplication = duplicate_string(Runner, FullScriptPath);
			*pCommandLine = duplicate_string(Runner, CommandLine);
		}
	}
	else {
		*pApplication = NULL;
		*pCommandLine = NULL;
	}

cleanup:
	return result;
}

static HRESULT RunProcessAndWait(PEXERUNNER Runner, LPSTR Application, LPSTR CommandLine, DWORD *Status)
{
	STARTUPINFOW StartupInfo = { 0 };
	PROCESS_INFORMATION ProcessInfo = { 0 };
	BOOL CreationResult;
	HRESULT result = S_OK;

	StartupInfo.cb = sizeof(StartupInfo);
	CreationResult = CreateProcess(Application, CommandLine, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS,
		NULL, NULL, &StartupInfo, &ProcessInfo);

	if (!CreationResult) {
		result = HRESULT_FROM_WIN32(GetLastError());
		set_errorlevel(Runner, 1, Status);
	}
	else {
		DWORD errorLevel;

		assert(ProcessInfo.hProcess);
		assert(ProcessInfo.hThread);
		WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
		if (!GetExitCodeProcess(ProcessInfo.hProcess, &errorLevel)) {
			result = HRESULT_FROM_WIN32(GetLastError());
			set_errorlevel(Runner, 1, Status);
		}
		CloseHandle(ProcessInfo.hProcess);
		CloseHandle(ProcessInfo.hThread);
		set_errorlevel(Runner, errorLevel, Status);
	}

	return result;
}

HRESULT RunExecutable(PEXERUNNER Runner, char *Application, char *CommandLine, DWORD *Status)
{
	LPSTR ApplicationName, NewCommandLine, FullPath;
	LPVOID PoolPtr;
	HRESULT result = S_OK;
	struct PathEntry *p = Runner->PathEntries;
	DWORD FullPathSize;

	PoolPtr = Runner->PoolPtr;
	FullPathSize = GetFullPathName(Application, 0, NULL, NULL);
	FullPath = (LPSTR)runner_alloc(Runner, sizeof(CHAR), sizeof(CHAR), FullPathSize + 1);
	if (!FullPath)
		return E_OUTOFMEMORY;
	GetFullPathName(Application, FullPathSize + 1, FullPath, NULL);
	result = find_executable(Runner, FullPath, NULL, CommandLine, &ApplicationName, &NewCommandLine);

	if (FAILED(result)) {
		Runner->PoolPtr = PoolPtr;
		do {
			LPVOID PoolPtr2 = Runner->PoolPtr;

			result = find_executable(Runner, Application, p->Path, CommandLine, &ApplicationName, &NewCommandLine);
			if (SUCCEEDED(result)) {
				break;
			}

			p = p->Next;
			Runner->PoolPtr = PoolPtr2;
		}
		while (p);
	}

	if (SUCCEEDED(result)) {
		result = RunProcessAndWait(Runner, ApplicationName, NewCommandLine, Status);
	}
	else {
		set_errorlevel(Runner, 1, Status);
	}

	Runner->PoolPtr = PoolPtr;
	return result;
}