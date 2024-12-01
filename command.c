#include "stdafx.h"
#include "operations.h"
#include "command.h"
#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include <windows.h>
enum
{
	ARCH_UNKNOWN,
	ARCH_X86,
	ARCH_X64,
	ARCH_ARM64,

	ARCH_COUNT
};

static int TestArch(void);
static HRESULT CommandIf(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandCopyFile(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandCopyDir(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandMove(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandRename(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandRemoveFile(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandRemoveRecurse(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandRemoveDir(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandMakeDir(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);
static HRESULT CommandRun(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);

static sds NextToken(char *CommandLine, size_t *pos)
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

HRESULT InitCommandRunner(PCMDRUNNER Runner)
{
	HRESULT result;
	Runner->ErrorLevel = 0;
	return S_OK;
}

HRESULT FreeCommandRunner(PCMDRUNNER Runner)
{
	HRESULT result = S_OK;
	Runner->ErrorLevel = 0;
	return S_OK;
}

HRESULT ExecuteCommand(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds command;
	size_t pos = 0;

	if (!Runner)
		return E_POINTER;
	if (!Status)
		return E_POINTER;
	*Status = 0;

	if (!CommandLine)
		return S_FALSE;
	if (!*CommandLine)
		return S_FALSE;

	command = NextToken(CommandLine, &pos);
	if (xstricmp(command, "Run") == 0) {
		result = CommandRun(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "If") == 0) {
		result = CommandIf(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "CopyFile") == 0) {
		result = CommandCopyFile(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "CopyDir") == 0) {
		result = CommandCopyDir(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "Move") == 0) {
		result = CommandMove(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "RemoveFile") == 0) {
		result = CommandRemoveFile(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "RemoveRecurse") == 0) {
		result = CommandRemoveRecurse(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "RemoveDir") == 0) {
		result = CommandRemoveDir(Runner, CommandLine + pos, Status);
	}
	else if (xstricmp(command, "MakeDir") == 0) {
		result = CommandMakeDir(Runner, CommandLine + pos, Status);
	}
	else {
		result = WPIW_E_COMMAND_NOT_FOUND;
	}
	Runner->ErrorLevel = *Status;

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

static HRESULT ParseArch(PCMDRUNNER Runner, char *CommandLine)
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

static HRESULT TestCondition(PCMDRUNNER Runner, char *CommandLine)
{
	size_t pos = 0;
	sds argument = NULL;
	HRESULT result = S_OK;

	argument = NextToken(CommandLine, &pos);
	if (xstricmp(argument, "Arch") == 0) {
		result = ParseArch(Runner, CommandLine + pos);
	}
	else {
		result = E_INVALIDARG;
	}

	sdsfree(argument);
	return result;
}

static HRESULT CommandIf(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
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

	if (SUCCEEDED(result = TestCondition(Runner, condition))) {
		if (result == WPIW_S_SKIP_COMMAND) 
			goto cleanup;

		result = ExecuteCommand(Runner, CommandLine + pos, Status);
	}

cleanup:
	if (condition)
		sdsfree(condition);
	return result;
}

typedef HRESULT(*ONE_ARG_CALLBACK)(char *Arg);
typedef HRESULT(*TWO_ARG_CALLBACK)(char *Arg1, char *Arg2);

static HRESULT CommandRun(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds application;
	size_t pos = 0;

	assert(Status);
	*Status = 1;

	application = NextToken(CommandLine, &pos);
	if (!application) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	{
		SHELLEXECUTEINFO sei = { 0 };
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.lpVerb = "open";
		sei.lpFile = application;
		sei.lpParameters = CommandLine + pos;
		sei.nShow = SW_SHOW;
		sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;

		if (!ShellExecuteEx(&sei)) {
			result = HRESULT_FROM_WIN32(GetLastError());
		}
		else {
			if (sei.hProcess) {
				WaitForSingleObject(sei.hProcess, INFINITE);
				GetExitCodeProcess(sei.hProcess, Status);
				CloseHandle(sei.hProcess);
			}
			else {
				result = HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
				*Status = 1;
			}
			Runner->ErrorLevel = *Status;
		}
	}

	sdsfree(application);
exit_fn:
	return result;
}

static inline HRESULT CommandOneArg(PCMDRUNNER Runner, char *CommandLine, DWORD *Status, ONE_ARG_CALLBACK cb)
{
	HRESULT result = S_OK;
	sds arg = NULL, arg2 = NULL;
	size_t pos = 0;

	assert(Status);
	*Status = 1;

	arg = NextToken(CommandLine, &pos);
	if (!arg) {
		return E_INVALIDARG;
	}
	if (!sdslen(arg)) {
		goto cleanup;
	}

	arg2 = NextToken(CommandLine, &pos);
	if (!arg2) {
		arg2 = E_OUTOFMEMORY;
		goto cleanup;
	}
	if (sdslen(arg2)) {
		result = E_INVALIDARG;
		goto cleanup2;
	}

	result = cb(arg);
	if (SUCCEEDED(result)) *Status = 0;

cleanup2:
	sdsfree(arg2);
cleanup:
	sdsfree(arg);
	return result;
}

static inline HRESULT CommandTwoArgs(PCMDRUNNER Runner, char *CommandLine, DWORD *Status, TWO_ARG_CALLBACK cb)
{
	HRESULT result = S_OK;
	sds first, second, third;
	size_t pos = 0;

	assert(Status);
	*Status = 1;

	first = NextToken(CommandLine, &pos);
	if (!first) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	second = NextToken(CommandLine, &pos);
	if (!second) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}
	if (!sdslen(second)) {
		result = E_INVALIDARG;
		goto cleanup2;
	}

	third = NextToken(CommandLine, &pos);
	if (!third) {
		result = E_OUTOFMEMORY;
		goto cleanup2;
	}
	if (sdslen(third)) {
		result = E_INVALIDARG;
		goto cleanup3;
	}

	result = cb(first, second);
	if (SUCCEEDED(result)) *Status = 0;

cleanup3:
	sdsfree(third);
cleanup2:
	sdsfree(second);
cleanup1:
	sdsfree(first);
exit_fn:
	return result;
}

static HRESULT CommandCopyFile(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandTwoArgs(Runner, CommandLine, Status, OpCopySingleFile);
}

static HRESULT CommandCopyDir(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandTwoArgs(Runner, CommandLine, Status, OpCopyDirectory);
}

static HRESULT CommandMove(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandTwoArgs(Runner, CommandLine, Status, OpMoveFile);
}

static HRESULT CommandRename(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandTwoArgs(Runner, CommandLine, Status, OpRenameFile);
}

static HRESULT CommandRemoveFile(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandOneArg(Runner, CommandLine, Status, OpRemoveFile);
}

static HRESULT CommandRemoveRecurse(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandOneArg(Runner, CommandLine, Status, OpRemoveRecurse);
}

static HRESULT CommandRemoveDir(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandOneArg(Runner, CommandLine, Status, OpRemoveDirectory);
}

static HRESULT CommandMakeDir(PCMDRUNNER Runner, char *CommandLine, DWORD *Status)
{
	return CommandOneArg(Runner, CommandLine, Status, OpMakeDirectory);
}