#pragma once

#include <windows.h>

typedef struct CommandRunner CMDRUNNER, *PCMDRUNNER;
struct CommandRunner
{
	DWORD     ErrorLevel;
};

HRESULT InitCommandRunner(PCMDRUNNER Runner);
HRESULT FreeCommandRunner(PCMDRUNNER Runner);
HRESULT ExecuteCommand(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);