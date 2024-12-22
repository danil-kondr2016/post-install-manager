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

static uint32_t hash(char *str);
static struct node *find(struct node **node, char *name, 
		struct arena *perm);
static bool key_is(struct node *node, char *name);

void repository_add_category(struct repository *repo, 
		struct category *category, struct arena *perm)
{
	struct node *result = repo->categories;
	size_t name_len;

	name_len = strlen(category->name);
	find(&result, category->name, perm);
	result->type = NODE_CATEGORY;
	result->cat = arena_new(perm, struct category);
	result->cat->name = arena_new(perm, char, name_len + 1);
	memcpy(result->cat->name, category->name, name_len);
}

struct category *repository_get_category(struct repository *repo,
		char *name)
{
	struct node *tree = repo->categories;

	if (find(&tree, name, NULL) && tree->type == NODE_CATEGORY)
		return tree->cat;
	return NULL;
}

static struct node *find(struct node **node, char *name,
		struct arena *perm)
{
	uint32_t h = hash(name);
	struct node **insert = node;

	while (*insert) {
		uint32_t ph = insert[0]->hash;
		switch ((h > ph) - (h < ph)) {
		case 0: if (key_is(insert[0], name)) return insert[0];
		case -1: insert = insert[0]->child + 0; break;
		case  1: insert = insert[0]->child + 1; break;
		}
	}

	if (perm) {
		*insert = arena_new(perm, struct node);
		(**insert).hash = h;
	}

	return insert[0];
}

static bool key_is(struct node *node, char *name)
{
	if (!node)
		return false;

	switch (node->type) {
	case NODE_CATEGORY: return node->cat && !strcmp(node->cat->name, name);
	}

	return false;
}

static uint32_t hash(char *str)
{
	uint32_t h = 0x100;
	uint8_t *p;

	for (p = (uint8_t *)str; *p != 0; p++) {
		h = 37*h + (*p);
	}

	return h;
}
