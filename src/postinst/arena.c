#include "arena.h"
#include "fatal.h"

#include <windows.h>

struct arena arena_create(ptrdiff_t size)
{
	struct arena result = {0};

	result.base = VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	if (!result.base)
		fatal_errorW(L"Out of memory", STATUS_NO_MEMORY);

	result.begin = result.base;
	result.end = result.begin + size;

	return result;
}

void arena_free(struct arena *arena)
{
	if (!arena)
		return;
	if (!arena->base)
		return;

	VirtualFree(arena->base, 0, MEM_RELEASE);
	arena->base = NULL;
	arena->begin = NULL;
	arena->end = NULL;
}

uint8_t *arena_alloc(struct arena *arena, 
		ptrdiff_t size, 
		ptrdiff_t align,
		ptrdiff_t count)
{
	ptrdiff_t padding = -(uintptr_t)arena->begin & (align - 1);
	ptrdiff_t available = arena->end - arena->begin - padding;

	if (available < 0 || count > available / size)
		fatal_errorW(L"Out of memory", STATUS_NO_MEMORY);

	uint8_t *p = arena->begin + padding;
	ptrdiff_t total = count * size;
	arena->begin += total + padding;

	for (ptrdiff_t i = 0; i < total; i++)
		p[i] = 0;
	return p;
}

wchar_t *u8_to_u16(char *ch, struct arena *arena)
{
	DWORD size;
	wchar_t *result;

	size = MultiByteToWideChar(CP_UTF8, 0, ch, -1, NULL, 0);
	result = arena_new(arena, wchar_t, size + 1);
	MultiByteToWideChar(CP_UTF8, 0, ch, -1, result, size);

	return result;
}
