#pragma once

#include "arena.h"

struct program {
	char *name;
	char *cmd;
	struct program *next;
};

struct repository {
	struct program *head;
	struct program *tail;
};

void repository_add(struct repository *repo, struct arena *perm,
		char *name,
		char *cmd);

uint32_t repository_parse(struct repository *repo, char *file_name,
		struct arena *perm,
		struct arena scratch);
