#include "repo.h"
#include <windows.h>

#include <string.h>

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
		prog->prev = repo->tail;
		repo->tail = prog;
	}

	return prog;
}

void repository_delete(struct repository *repo, struct program *prog)
{
	if (repo->head == repo->tail && prog == repo->head) {
		repo->head = NULL;
		repo->tail = NULL;
	}
	else if (prog == repo->head) {
		repo->head = repo->head->next;
		repo->head->prev = NULL;
	}
	else if (prog == repo->tail) {
		repo->tail = repo->tail->prev;
		repo->tail->next = NULL;
	}
	else {
		struct program *prev, *next;
		prev = prog->prev;
		next = prog->next;

		prev->next = next;
		next->prev = prev;
	}
}
