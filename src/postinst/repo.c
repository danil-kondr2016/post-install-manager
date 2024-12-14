#include "repo.h"
#include <windows.h>

#include <string.h>
#include <csv.h>

void repository_add(struct repository *repo, struct arena *perm,
		char *name,
		char *cmd)
{
	struct program *prog;
	size_t name_len, cmd_len;

	name_len = strlen(name);
	cmd_len = strlen(cmd);

	prog = arena_new(perm, struct program);
	prog->name = arena_new(perm, char, name_len+1);
	memcpy(prog->name, name, name_len);
	prog->cmd = arena_new(perm, char, cmd_len+1);
	memcpy(prog->cmd, cmd, cmd_len);

	if (!repo->head && !repo->tail) {
		repo->tail = repo->head = prog;
	}
	else {
		repo->tail->next = prog;
		repo->tail = prog;
	}
}

#define FILE_BUFFER_SIZE 65536

struct state {
	struct repository *repo;
	struct arena *perm;
	intptr_t column_index;
	char *name, *cmd;
};

static void _on_field(void *entry, size_t len, void *data);
static void _on_entry(int ch, void *data);

uint32_t repository_parse(struct repository *repo, char *file_name,
		struct arena *perm,
		struct arena scratch)
{
	struct csv_parser s = {0};
	HANDLE file;
	wchar_t *file_nameW;
	uint32_t file_nameW_size;
	uint8_t *file_buffer;
	DWORD read_count, char_processed;
	uint32_t result;
	struct state state = {repo, perm, 0, NULL, NULL};

	file_buffer = arena_new(&scratch, uint8_t, FILE_BUFFER_SIZE);
	file_nameW_size = MultiByteToWideChar(CP_UTF8, 0, file_name, -1, NULL, 0);
	file_nameW = arena_new(&scratch, wchar_t, file_nameW_size + 1);
	MultiByteToWideChar(CP_UTF8, 0, file_name, -1, file_nameW, file_nameW_size);

	file = CreateFileW(file_nameW, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0xC0070000 | (GetLastError() & 0xFFFF);

	csv_init(&s, CSV_STRICT | CSV_STRICT_FINI | CSV_APPEND_NULL);

	do {
		if (!ReadFile(file, file_buffer, FILE_BUFFER_SIZE, &read_count, NULL)) {
			result = 0xC0070000 | (GetLastError() & 0xFFFF);	
			goto cleanup2;
		}
		if (!read_count)
			break;

		char_processed = csv_parse(&s, file_buffer, read_count, _on_field, _on_entry, &state);
		if (read_count != char_processed) {
			switch (csv_error(&s)) {
			case CSV_EPARSE:
				result = 0xE7F00001;
				break;
			case CSV_ENOMEM:
			case CSV_ETOOBIG:
				result = E_OUTOFMEMORY;
				break;
			}
			goto cleanup2;
		}
	}
	while (read_count > 0);

	if (-1 == csv_fini(&s, _on_field, _on_entry, repo)) {
		switch (csv_error(&s)) {
		case CSV_EPARSE:
			result = 0xE7F00001;
			break;
		case CSV_ENOMEM:
		case CSV_ETOOBIG:
			result = E_OUTOFMEMORY;
			break;
		}
	}

	return 0;
cleanup2:
	csv_free(&s);
cleanup1:
	CloseHandle(file);
	return result;
}

static void _on_field(void *field, size_t len, void *data)
{
	struct state state = {0};
	memcpy(&state, data, sizeof(struct state));
	
	switch (state.column_index) {
	case 0:
		state.name = arena_new(state.perm, char, len+1);
		memcpy(state.name, field, len);
		break;
	case 1:
		state.cmd = arena_new(state.perm, char, len+1);
		memcpy(state.cmd, field, len);
		break;
	}
	state.column_index++;	

	memcpy(data, &state, sizeof(struct state));
}

static void _on_entry(int ch, void *data)
{
	struct state state = {0};
	memcpy(&state, data, sizeof(struct state));

	state.column_index = 0;
	repository_add(state.repo, state.perm, state.name, state.cmd);

	memcpy(data, &state, sizeof(struct state));
}
