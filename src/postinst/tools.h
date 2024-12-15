#pragma once
#ifndef _TOOLS_H_
#define _TOOLS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

int xstricmp(const char *s1, const char *s2);
int xstrnicmp(const char *s1, const char *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif
