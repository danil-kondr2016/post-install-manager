#pragma once

#include <windows.h>

/* Skip execution of command */
#define S_SKIP_COMMAND              0x27F08000
/* Failed to execute command */
#define E_COMMAND_ERROR             0xA7F00001

HRESULT ExecuteCommand(char *CommandLine, DWORD *Status);
HRESULT ExecuteArgcArgv(int argc, char **argv, DWORD *Status);