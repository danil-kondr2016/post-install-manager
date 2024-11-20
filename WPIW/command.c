#include "command.h"

#include "sds.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

enum
{
	ARCH_UNKNOWN,
	ARCH_X86,
	ARCH_X64,
	ARCH_ARM64,
};

static int xstricmp(const char *a, const char *b)
{
	char x, y;
	int diff = 0;

	do {
		x = tolower(*a);
		y = tolower(*b);
		
		++a;
		++b;
	} while (x && (x==y));

	return x - y;
}

static int xwcsicmp(const wchar_t *a, const wchar_t *b)
{
	wchar_t x, y;
	int diff = 0;

	do {
		x = towlower(*a);
		y = towlower(*b);

		++a;
		++b;
	}
	while (x && (x == y));

	return x - y;
}

static int TestArch(void);
static HRESULT RunExecutable(int argc, char **argv, DWORD *Status);
static HRESULT RunIfArchIs(int arch, int argc, char **argv, DWORD *Status);

HRESULT ExecuteCommand(char *CommandLine, DWORD *Status)
{
	int argc;
	sds *argv;
	HRESULT result = S_OK;
	
	if (!Status)
		return E_POINTER;
	*Status = 0;

	if (!CommandLine)
		return S_FALSE;
	if (!*CommandLine)
		return S_FALSE;

	argv = sdssplitargs(CommandLine, &argc);
	if (!argv)
		return E_OUTOFMEMORY;
	if (argc == 0)
		goto cleanup;

	result = ExecuteArgcArgv(argc, argv, Status);

cleanup:
	sdsfreesplitres(argv, argc);
	return result;
}

HRESULT ExecuteArgcArgv(int argc, char **argv, DWORD *Status)
{
	HRESULT result = S_OK;

	if (!Status)
		return E_POINTER;
	*Status = 0;

	if (!argv)
		return S_FALSE;
	if (argc == 0)
		return S_FALSE;

	if (xstricmp(argv[0], "RunIfArchIsX86") == 0) {
		result = RunIfArchIs(ARCH_X86, argc - 1, argv + 1, Status);
	}
	else if (xstricmp(argv[0], "RunIfArchIsX64") == 0) {
		result = RunIfArchIs(ARCH_X64, argc - 1, argv + 1, Status);
	}
	else {
		result = RunExecutable(argc, argv, Status);
	}

	return result;
}

static int TestArch(void)
{
	SYSTEM_INFO info;

	GetNativeSystemInfo(&info);
	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		return ARCH_X86;
	case PROCESSOR_ARCHITECTURE_AMD64:
		return ARCH_X64;
	case PROCESSOR_ARCHITECTURE_ARM64:
		return ARCH_ARM64;
	default:
		return ARCH_UNKNOWN;
	}
}

static LPWSTR UTF8ToWideCharAlloc(char *source)
{
	DWORD resultSize;
	LPWSTR result;
	int i;

	resultSize = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
	result = (LPWSTR)calloc(resultSize + 1, sizeof(WCHAR));
	if (!result)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, source, -1, result, resultSize);
	result[resultSize] = 0;

	return result;
}

static LPWSTR ArgvToCommandLine(int argc, char **argv)
{
	sds commandLine = NULL;
	int i;
	LPWSTR result = NULL;
	DWORD resultSize;

	if (!argv || argc == 0) {
		result = (LPWSTR)calloc(1, sizeof(WCHAR));
		return result;
	}

	commandLine = sdsnew("");
	if (!commandLine)
		return NULL;

	for (i = 0; i < argc; i++) {
		if (strchr(argv[i], ' ') || strchr(argv[i], '"'))
			commandLine = sdscatrepr(commandLine, argv[i], strlen(argv[i]));
		else
			commandLine = sdscat(commandLine, argv[i]);
		commandLine = sdscat(commandLine, " ");
	}

	if (!commandLine)
		return NULL;

	resultSize = MultiByteToWideChar(CP_UTF8, 0, commandLine, -1, NULL, 0);
	result = (LPWSTR)calloc(resultSize, sizeof(WCHAR));
	if (!result) {
		goto cleanup;
	}
	MultiByteToWideChar(CP_UTF8, 0, commandLine, -1, result, resultSize);

cleanup:
	sdsfree(commandLine);
	return result;
}

static HRESULT RunExecutable(int argc, char **argv, DWORD *Status)
{
	LPWSTR absolutePath, commandLine;
	SHELLEXECUTEINFOW shellExecuteInfo = { 0 };
	HRESULT result = 0;

	assert(Status);
	*Status = 0;
	if (!argv)
		return S_OK;
	if (argc == 0)
		return S_OK;

	shellExecuteInfo.cbSize = sizeof(shellExecuteInfo);

	absolutePath = UTF8ToWideCharAlloc(argv[0]);
	if (!absolutePath) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	commandLine = ArgvToCommandLine(argc - 1, argv + 1);
	if (!commandLine)
		return E_OUTOFMEMORY;
	
	shellExecuteInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
	shellExecuteInfo.lpVerb = L"open";
	shellExecuteInfo.lpFile = absolutePath;
	shellExecuteInfo.lpParameters = commandLine;
	shellExecuteInfo.nShow = SW_SHOW;

	if (!ShellExecuteExW(&shellExecuteInfo))  {
		result = HRESULT_FROM_WIN32(GetLastError());
		goto cleanup2;
	}

	if (!shellExecuteInfo.hProcess || WAIT_FAILED == WaitForSingleObject(shellExecuteInfo.hProcess, INFINITE)) {
		result = HRESULT_FROM_WIN32(GetLastError());
	}
	else {
		GetExitCodeProcess(shellExecuteInfo.hProcess, Status);
	}

cleanup2:
	free(commandLine);
cleanup1:
	free(absolutePath);
exit_fn:
	return result;
}

static HRESULT RunIfArchIs(int arch, int argc, char **argv, DWORD *Status)
{
	int native_arch;

	assert(Status);
	*Status = 0;

	native_arch = TestArch();
	if (native_arch != ARCH_UNKNOWN && arch != native_arch)
		return S_SKIP_COMMAND;
	if (native_arch == ARCH_UNKNOWN)
		return E_COMMAND_ERROR;

	return ExecuteArgcArgv(argc, argv, Status);
}