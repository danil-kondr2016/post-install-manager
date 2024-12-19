#pragma once

#include <stddef.h>
#include <stdint.h>

#define PIM_STATUS_SKIPPED       UINT32_C(0x27F10000)
#define PIM_ERROR_INVALID_FORMAT UINT32_C(0xE7F00001)
#define PIM_ERROR_FAIL           UINT32_C(0xE7F1FFFF)

#ifndef IS_PIM
#define IS_PIM(x) (((uint32_t)(x) & UINT32_C(0x2FF00000)) == UINT32_C(0x27F00000))
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) ((uint32_t) (status) >= 0)
#endif

#ifndef NT_INFORMATION
#define NT_INFORMATION(Status) ((((uint32_t)(Status)) >> 30) == 1)
#endif

#ifndef NT_WARNING
#define NT_WARNING(Status) ((((uint32_t)(Status)) >> 30) == 2)
#endif

#ifndef NT_ERROR
#define NT_ERROR(Status) ((((uint32_t)(Status)) >> 30) == 3)
#endif

#include <ntstatus.h>
