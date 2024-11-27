#pragma once

#include <wchar.h>
#include "sds.h"

wchar_t *UTF8ToWideCharAlloc(char *source);
sds WideCharToSdsAlloc(wchar_t *source);