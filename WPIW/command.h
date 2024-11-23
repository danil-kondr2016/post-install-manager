#pragma once

#include <windows.h>

/* Skip execution of command */
#define WPIW_S_SKIP_COMMAND  (HRESULT)0x27F08000L
/* Command not found */
#define WPIW_E_COMMAND_NOT_FOUND (HRESULT)0xE7F00001L
/* Failed to execute command*/
#define WPIW_E_FAILED_TO_EXECUTE (HRESULT)0xE7F00002L
/* Failed to install program */
#define WPIW_E_FAILED_TO_INSTALL (HRESULT)0xE7F10001L

HRESULT ExecuteCommand(char *CommandLine, DWORD *Status);