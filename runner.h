#pragma once

#include <windows.h>

enum ScriptInterpreter
{
	INTERPRETER_NULL,
	INTERPRETER_CMD,
	INTERPRETER_WSCRIPT,
	INTERPRETER_POWERSHELL,

	INTERPRETER_COUNT
};

struct ExecutableRunner
{
	LPBYTE PoolStart;
	LPBYTE PoolPtr;
	LPBYTE PoolEnd;

	LPSTR  Interpreters[INTERPRETER_COUNT];
	LPVOID PathEntries;

	DWORD  ErrorLevel;
};
typedef struct ExecutableRunner EXERUNNER, *PEXERUNNER;

HRESULT InitExecutableRunner(PEXERUNNER Runner);
HRESULT FreeExecutableRunner(PEXERUNNER Runner);
HRESULT RunExecutable(PEXERUNNER Runner, char *Application, char *CommandLine, DWORD *Status);