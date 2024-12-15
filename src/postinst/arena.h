#pragma once

#include <stddef.h>
#include <stdint.h>

struct arena {
	uint8_t *base;
	uint8_t *begin;
	uint8_t *end;
};

struct arena arena_create(ptrdiff_t size);
void arena_free(struct arena *arena);

uint8_t *arena_alloc(struct arena *arena, 
		ptrdiff_t size, 
		ptrdiff_t align, 
		ptrdiff_t count);

#define arena_new(...) arena_newx(__VA_ARGS__, arena_new3, arena_new2)(__VA_ARGS__)
#define arena_newx(a, b, c, d, ...) d
#define arena_new2(a, t) (t *)arena_alloc(a, sizeof(t), _Alignof(t), 1)
#define arena_new3(a, t, n) (t *)arena_alloc(a, sizeof(t), _Alignof(t), n)

wchar_t *u8_to_u16(char *ch, struct arena *arena);
