#include "repo.h"
#include <windows.h>

#include <string.h>
#include <csv.h>

struct program *repository_add(struct repository *repo, struct arena *perm)
{
	struct program *prog;
	size_t name_len, cmd_len;

	prog = arena_new(perm, struct program);

	if (!repo->head && !repo->tail) {
		repo->tail = repo->head = prog;
	}
	else {
		repo->tail->next = prog;
		repo->tail = prog;
	}

	return prog;
}
