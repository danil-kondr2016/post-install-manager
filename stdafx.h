#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "win32_utf8/win32_utf8.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "cwalk.h"
#include "sds.h"

/* Skip execution of command */
#define WPIW_S_SKIP_COMMAND         (HRESULT)0x27F08000L
/* Command not found */
#define WPIW_E_COMMAND_NOT_FOUND    (HRESULT)0xE7F00001L
/* Run: Failed to find executable */
#define WPIW_E_EXECUTABLE_NOT_FOUND (HRESULT)0xE7F00002L
/* Failed to install program */
#define WPIW_E_FAILED_TO_INSTALL    (HRESULT)0xE7F10001L