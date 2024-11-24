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
	ARCH_COUNT,
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
static HRESULT RunExecutable(char *FileName, char *CommandLine, DWORD *Status);
static HRESULT RunIfArchIs(int arch, char *CommandLine, DWORD *Status);
static HRESULT CommandIf(char *CommandLine, DWORD *Status);

sds NextToken(char *CommandLine, size_t *pos)
{
	sds result;
	size_t length;
	char *p;

	assert(pos);
	result = sdsempty();
	length = strlen(CommandLine);

	if (*pos >= length)
		goto exit_fn;

	p = CommandLine + (*pos);
	while (*p && isspace(*p)) p++;
	if (*p) {
		int in_quotes = 0;
		int done = 0;
		while (!done) {
			if (in_quotes) {
				if (p[0] == '\\' && p[1]) {
					p++;
					result = sdscatlen(result, p, 1);
				}
				else if (*p == '"') {
					if (p[1] && !isspace(p[1]))
						goto error;
					done = 1;
				}
				else if (!*p) {
					goto error;
				}
				else {
					result = sdscatlen(result, p, 1);
				}
			}
			else {
				switch (*p) {
				case ' ':
				case '\t':
				case '\r':
				case '\n':
				case '\0':
					done = 1;
					break;
				case '"':
					in_quotes = 1;
					break;
				default:
					result = sdscatlen(result, p, 1);
					break;
				}
			}
			if (*p) p++;
		}
	}
	
	*pos = p - CommandLine;

exit_fn:
	return result;
error:
	sdsfree(result);
	return NULL;
}

HRESULT ExecuteCommand(char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds command;
	size_t pos = 0;

	if (!Status)
		return E_POINTER;
	*Status = 0;

	if (!CommandLine)
		return S_FALSE;
	if (!*CommandLine)
		return S_FALSE;

	command = NextToken(CommandLine, &pos);
	if (xstricmp(command, "Run") == 0) {
		sds fileName = NextToken(CommandLine, &pos);
		result = RunExecutable(fileName, CommandLine + pos, Status);
		sdsfree(fileName);
	}
	else if (xstricmp(command, "If") == 0) {
		result = CommandIf(CommandLine + pos, Status);
	}
	else {
		result = WPIW_E_COMMAND_NOT_FOUND;
	}

	if (command) 
		sdsfree(command);
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

static HRESULT RunExecutable(char *FileName, char *CommandLine, DWORD *Status)
{
	LPWSTR fileName, commandLine;
	SHELLEXECUTEINFOW shellExecuteInfo = { 0 };
	HRESULT result = 0;

	assert(Status);
	*Status = 0;
	if (!FileName)
		return S_OK;
	if (!*FileName)
		return S_OK;

	shellExecuteInfo.cbSize = sizeof(shellExecuteInfo);

	fileName = UTF8ToWideCharAlloc(FileName);
	if (!fileName) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	commandLine = UTF8ToWideCharAlloc(CommandLine);
	if (!commandLine) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}

	shellExecuteInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
	shellExecuteInfo.lpVerb = L"open";
	shellExecuteInfo.lpFile = fileName;
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
	free(fileName);
exit_fn:
	return result;
}

static HRESULT RunIfArchIs(int arch, char *CommandLine, DWORD *Status)
{
	int native_arch;
	HRESULT result;

	assert(Status);
	*Status = 0;

	if (arch == ARCH_UNKNOWN)
		return S_FALSE;

	native_arch = TestArch();
	if (native_arch != ARCH_UNKNOWN && arch != native_arch)
		result = WPIW_S_SKIP_COMMAND;
	if (native_arch == ARCH_UNKNOWN)
		result = WPIW_E_FAILED_TO_EXECUTE;

	result = ExecuteCommand(CommandLine, Status);
	return result;
}

static HRESULT ParseArch(char *CommandLine)
{
	sds archList = NULL;
	sds *values;
	int count, i;
	int native_arch;
	size_t pos = 0;
	HRESULT result = WPIW_S_SKIP_COMMAND;
	unsigned char possible_arches[ARCH_COUNT] = { 0 };

	archList = sdsnew(CommandLine);

	sdstrim(archList, ",");
	values = sdssplitlen(archList, sdslen(archList), ",", 1, &count);
	for (i = 0; i < count; i++) {
		if (values[i])
			sdstrim(values[i], " \t\r\n");
		if (sdslen(values[i]) == 0) {
			sdsfree(values[i]);
			values[i] = NULL;
			continue;
		}

		if (!possible_arches[ARCH_X86] && xstricmp(values[i], "X86") == 0) {
			possible_arches[ARCH_X86]++;
			break;
		}
		else if (!possible_arches[ARCH_X64] && xstricmp(values[i], "X64") == 0) {
			possible_arches[ARCH_X64]++;
		}
		else if (!possible_arches[ARCH_X64] && xstricmp(values[i], "AMD64") == 0) {
			possible_arches[ARCH_X64]++;
		}
		else if (!possible_arches[ARCH_X64] && xstricmp(values[i], "X86_64") == 0) {
			possible_arches[ARCH_X64]++;
		}
		else if (!possible_arches[ARCH_X64] && xstricmp(values[i], "X86-64") == 0) {
			possible_arches[ARCH_X64]++;
		}
		else if (!possible_arches[ARCH_ARM64] && xstricmp(values[i], "ARM64") == 0) {
			possible_arches[ARCH_ARM64]++;
		}
		else {
			result = E_INVALIDARG;
			goto cleanup2;
		}
	}

	native_arch = TestArch();
	for (i = ARCH_UNKNOWN; i < ARCH_COUNT; i++) {
		if (possible_arches[i] && i == native_arch) {
			result = S_OK;
			break;
		}
	}

cleanup2:
	sdsfreesplitres(values, count);
cleanup1:
	sdsfree(archList);
	return result;
}

static HRESULT TestCondition(char *CommandLine)
{
	size_t pos = 0;
	sds argument = NULL;
	HRESULT result = S_OK;

	argument = NextToken(CommandLine, &pos);
	if (xstricmp(argument, "Arch") == 0) {
		result = ParseArch(CommandLine + pos);
	}
	else {
		result = E_INVALIDARG;
	}

	sdsfree(argument);
	return result;
}

static HRESULT CommandIf(char *CommandLine, DWORD *Status)
{
	sds argument = NULL;
	sds condition = NULL;
	size_t pos = 0;
	HRESULT result = S_OK;
	
	condition = sdsempty();
	do {
		argument = NextToken(CommandLine, &pos);
		sdstrim(argument, " \t\r\n");
		if (xstricmp(argument, "Then") == 0) {
			sdsfree(argument);
			argument = NULL;
			break;
		}

		condition = sdscatsds(condition, argument);
		condition = sdscatlen(condition, " ", 1);

		sdsfree(argument);
		argument = NULL;
	}
	while (1);

	if (SUCCEEDED(result = TestCondition(condition))) {
		if (result == WPIW_S_SKIP_COMMAND) 
			goto cleanup;

		result = ExecuteCommand(CommandLine + pos, Status);
	}

cleanup:
	if (condition)
		sdsfree(condition);
	return result;
}