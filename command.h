#pragma once

#include <windows.h>
#include "runner.h"

typedef struct CommandRunner CMDRUNNER, *PCMDRUNNER;
struct CommandRunner
{
	EXERUNNER ExeRunner;
	DWORD     ErrorLevel;
};

HRESULT InitCommandRunner(PCMDRUNNER Runner);
HRESULT FreeCommandRunner(PCMDRUNNER Runner);
HRESULT ExecuteCommand(PCMDRUNNER Runner, char *CommandLine, DWORD *Status);