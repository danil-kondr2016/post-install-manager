#include "WPIW.h"
#include "command.h"
#include "utils.h"
#include "operations.h"
#include "tests.h"
#include "execute.h"

#include "sds.h"
#include "sdsalloc.h"

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
static HRESULT CommandIf(char *CommandLine, DWORD *Status);
static HRESULT CommandCopyFile(char *CommandLine, DWORD *Status);
static HRESULT CommandCopyDir(char *CommandLine, DWORD *Status);
static HRESULT CommandMove(char *CommandLine, DWORD *Status);
static HRESULT CommandRename(char *CommandLine, DWORD *Status);
static HRESULT CommandRemoveFile(char *CommandLine, DWORD *Status);
static HRESULT CommandRemoveDir(char *CommandLine, DWORD *Status);

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

sds *SplitTokens(char *CommandLine, int *Count)
{
	sds *result = NULL;
	sds token;
	size_t pos = 0;

	assert(Count);
	(*Count) = 0;
	result = s_malloc(sizeof(sds));
	if (!result)
		return result;
	*result = NULL;

	do {
		sds *result_new;
		result_new = s_realloc(result, (*Count + 2) * sizeof(sds));
		if (!result_new) {
			sdsfreesplitres(result, *Count - 1);
			return NULL;
		}
		result = result_new;

		token = NextToken(CommandLine, &pos);
		if (!token) {
			sdsfreesplitres(result, *Count - 1);
			return NULL;
		}
		if (!sdslen(token)) {
			break;
		}
		result[*Count] = token;
		result[*Count + 1] = NULL;

		(*Count)++;
	}
	while (1);

	return result;
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
		result = Run(fileName, CommandLine + pos, Status);
		sdsfree(fileName);
	}
	else if (xstricmp(command, "If") == 0) {
		result = CommandIf(CommandLine + pos, Status);
	}
	else if (xstricmp(command, "CopyFile") == 0) {
		result = CommandCopyFile(CommandLine + pos, Status);
	}
	else if (xstricmp(command, "CopyDir") == 0) {
		result = CommandCopyDir(CommandLine + pos, Status);
	}
	else if (xstricmp(command, "Move") == 0) {
		result = CommandMove(CommandLine + pos, Status);
	}
	else if (xstricmp(command, "RemoveFile") == 0) {
		result = CommandRemoveFile(CommandLine + pos, Status);
	}
	else if (xstricmp(command, "RemoveDir") == 0) {
		result = CommandRemoveDir(CommandLine + pos, Status);
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

static HRESULT CommandCopyFile(char *CommandLine, DWORD *Status)
{
	sds *values;
	int count;
	HRESULT result = S_OK;

	assert(Status);
	*Status = 1;

	values = SplitTokens(CommandLine, &count);
	if (!values) {
		return E_OUTOFMEMORY;
	}

	if (count < 2) {
		result = E_INVALIDARG;
	}
	else if (count == 2) {
		result = OpCopySingleFile(values[0], values[1]);
	}
	else if (count > 2) {
		sds destination = values[count - 1];
		int i;

		if (!TestIsDirectory(destination)) {
			result = HRESULT_FROM_WIN32(ERROR_CANNOT_COPY);
			goto cleanup;
		}

		for (i = 0; i < count - 1; i++) {
			result = OpCopySingleFile(values[i], destination);
			if (FAILED(result)) {
				break;
			}
		}
	}

	if (SUCCEEDED(result))
		*Status = 0;

cleanup:
	sdsfreesplitres(values, count);
	return result;
}

static HRESULT CommandCopyDir(char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds *values;
	int count;
	
	assert(Status);
	*Status = 1;

	values = SplitTokens(CommandLine, &count);
	if (!values) {
		return E_OUTOFMEMORY;
	}

	if (count == 2) {
		if (!TestIsDirectory(values[0]) || !TestIsDirectory(values[1])) {
			result = HRESULT_FROM_WIN32(ERROR_CANNOT_COPY);
			goto cleanup;
		}
		result = OpCopyDirectory(values[0], values[1]);
	}
	else {
		result = E_INVALIDARG;
	}

	if (SUCCEEDED(result))
		*Status = 0;

cleanup:
	sdsfreesplitres(values, count);
	return result;
}

// TODO unify with CommandCopyFile
static HRESULT CommandMove(char *CommandLine, DWORD *Status)
{
	sds *values;
	int count;
	HRESULT result = S_OK;

	assert(Status);
	*Status = 1;

	values = SplitTokens(CommandLine, &count);
	if (!values) {
		return E_OUTOFMEMORY;
	}

	if (count < 2) {
		result = E_INVALIDARG;
	}
	else if (count == 2) {
		result = OpMoveFile(values[0], values[1]);
	}
	else if (count > 2) {
		sds destination = values[count - 1];
		int i;

		if (!TestIsDirectory(destination)) {
			result = HRESULT_FROM_WIN32(ERROR_CANNOT_COPY);
			goto cleanup;
		}

		for (i = 0; i < count - 1; i++) {
			result = OpMoveFile(values[i], destination);
			if (FAILED(result)) {
				break;
			}
		}
	}

	if (SUCCEEDED(result))
		*Status = 0;

cleanup:
	sdsfreesplitres(values, count);
	return result;
}

static HRESULT CommandRename(char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds path, newName, third;
	size_t pos = 0;

	assert(Status);
	*Status = 1;

	path = NextToken(CommandLine, &pos);
	if (!path) {
		result = E_OUTOFMEMORY;
		goto exit_fn;
	}

	newName = NextToken(CommandLine, &pos);
	if (!newName) {
		result = E_OUTOFMEMORY;
		goto cleanup1;
	}
	if (!sdslen(newName)) {
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

	result = OpRenameFile(path, newName);
	if (SUCCEEDED(result)) *Status = 0;
	
cleanup3:
	sdsfree(third);
cleanup2:
	sdsfree(newName);
cleanup1:
	sdsfree(path);
exit_fn:
	return result;
}

static HRESULT CommandRemoveFile(char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds toRemove = NULL;
	size_t pos = 0;

	assert(Status);
	*Status = 1;

	while (SUCCEEDED(result)) {
		toRemove = NextToken(CommandLine, &pos);
		if (!toRemove) {
			return E_INVALIDARG;
		}
		if (!sdslen(toRemove)) {
			sdsfree(toRemove);
			break;
		}

		result = OpRemoveFile(toRemove);
		sdsfree(toRemove);
	}

	if (SUCCEEDED(result)) *Status = 0;
	return result;
}

// TODO unify with CommandRemoveFile
static HRESULT CommandRemoveDir(char *CommandLine, DWORD *Status)
{
	HRESULT result = S_OK;
	sds toRemove = NULL;
	size_t pos = 0;

	assert(Status);
	*Status = 1;

	while (SUCCEEDED(result)) {
		toRemove = NextToken(CommandLine, &pos);
		if (!toRemove) {
			return E_INVALIDARG;
		}
		if (!sdslen(toRemove)) {
			sdsfree(toRemove);
			break;
		}

		result = OpRemoveDirectory(toRemove);
		sdsfree(toRemove);
	}

	if (SUCCEEDED(result)) *Status = 0;
	return result;
}