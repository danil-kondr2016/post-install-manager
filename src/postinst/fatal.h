#pragma once

#include <stdint.h>
#include <wchar.h>

void fatal_errorW(wchar_t *message, uint32_t exit_code);
void error_msgW(void *window, uint32_t exit_code);
