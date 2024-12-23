#include "repo.h"
#include "tools.h"
#include "errors.h"

#include <expat.h>
#include <windows.h>
#include <stdbool.h>

#define FILE_BUFFER_SIZE 65536

#ifdef XML_UNICODE
#error This program oriented only on char-based version of Expat.
#endif

struct parser_state {
	struct arena *perm;
	struct repository *repo;
	struct program *prog;
	union command *cmd;
	union command *cmd_head;
	union command *cmd_tail;
	struct category cat;
	uint32_t state;
	bool has_programs : 1;
	bool has_categories : 1;
};

enum {
	ST_INIT,
	ST_REPO,
	
	ST_CATEGORIES,
	ST_CAT,
	ST_CAT_NAME,

	ST_PROGRAMS,
	ST_PROG,
	ST_PROG_NAME,
	ST_PROG_CAT,

	ST_CMDS,
	ST_CMD,

	ST_CMD_ARCH,
	ST_CMD_OS,
	ST_CMD_ALERT,
	ST_CMD_FAIL,

	ST_CMD_EXEC,
	ST_CMD_EXEC_FILE,
	ST_CMD_EXEC_ARGS,

	ST_CMD_CMD,

	ST_CMD_MKDIR,
	ST_CMD_RMDIR,
	ST_CMD_RMFILE,

	ST_CMD_MOVE,
	ST_CMD_MOVE_FROM,
	ST_CMD_MOVE_TO,

	ST_CMD_CPDIR,
	ST_CMD_CPDIR_FROM,
	ST_CMD_CPDIR_TO,
	
	ST_CMD_CPFILE,
	ST_CMD_CPFILE_FROM,
	ST_CMD_CPFILE_TO,

	ST_CMD_RENAME,
	ST_CMD_RENAME_PATH,
	ST_CMD_RENAME_NEW_NAME,

	ST_END,
	ST_INVALID = 0xFFFFFFFFUL
};

void XMLCALL on_element_start(struct parser_state *ps,
		const XML_Char *name,
		const XML_Char **atts);

void XMLCALL on_element_end(struct parser_state *ps,
		const XML_Char *name);

void XMLCALL on_char_data(struct parser_state *ps,
		const XML_Char *s,
		int len);

bool validate_categories(struct repository *repo);

uint32_t repository_parse(struct repository *repo, char *file_name,
		struct arena *perm,
		struct arena scratch)
{
	XML_Parser parser;
	struct arena old_perm = *perm;
	struct parser_state ps = {0};
	HANDLE file;
	wchar_t *file_nameW;
	uint8_t *file_buffer;
	uint32_t file_nameW_size;
	uint32_t result = 0;

	ps.perm = perm;
	ps.repo = repo;

	parser = XML_ParserCreate("UTF-8");
	if (!parser)
		return STATUS_NO_MEMORY;
	XML_SetUserData(parser, &ps);
	XML_SetElementHandler(parser, 
		(XML_StartElementHandler)on_element_start,
		(XML_EndElementHandler)on_element_end);
	XML_SetCharacterDataHandler(parser,
		(XML_CharacterDataHandler)on_char_data);

	file_nameW_size = MultiByteToWideChar(CP_UTF8, 0, file_name, -1, NULL, 0);
	file_nameW = arena_new(&scratch, wchar_t, file_nameW_size + 1);
	MultiByteToWideChar(CP_UTF8, 0, file_name, -1, file_nameW, file_nameW_size);

	file = CreateFileW(file_nameW, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == file) {
		result = NTSTATUS_FROM_WIN32(GetLastError());
		goto cleanup1;
	}

	for (;;) {
		DWORD read_count = 0;	
		void *file_buffer = XML_GetBuffer(parser, FILE_BUFFER_SIZE);
		if (!file_buffer) {
			result = STATUS_NO_MEMORY;
			goto cleanup2;
		}

		if (!ReadFile(file, file_buffer, FILE_BUFFER_SIZE, &read_count, NULL)) {
			XML_StopParser(parser, 0);
			result = NTSTATUS_FROM_WIN32(GetLastError());
			goto cleanup2;
		}

		if (!XML_ParseBuffer(parser, read_count, read_count == 0) || ps.state == ST_INVALID) {
			XML_StopParser(parser, 0);

			memset(repo, 0, sizeof(*repo));
			*perm = old_perm;
			result = PIM_ERROR_INVALID_FORMAT;
			goto cleanup2;
		}

		if (read_count == 0)
			break;
	}

	if (!validate_categories(repo)) {
		memset(repo, 0, sizeof(*repo));
		*perm = old_perm;
		result = PIM_ERROR_INVALID_FORMAT;
	}

cleanup2:
	CloseHandle(file);
cleanup1:
	XML_ParserFree(parser);
	return result;
}

bool validate_categories(struct repository *repo)
{
	for (struct program *prog = repo->head; prog; prog = prog->next) {
		if (!prog->category)
			continue;

		if (!repository_get_category(repo, prog->category))
			return false;
	}

	return true;
}

void XMLCALL on_element_start(struct parser_state *ps,
		const XML_Char *name,
		const XML_Char **atts)
{
	switch (ps->state) {
	case ST_INIT:
		if (!xstricmp(name, "pim-repository"))
			ps->state = ST_REPO;
		else
			ps->state = ST_INVALID;
		break;
	case ST_REPO:
		if (!ps->has_programs && !xstricmp(name, "programs")) {
			ps->state = ST_PROGRAMS;
			ps->has_programs = true;
		}
		else if (!ps->has_categories && !xstricmp(name, "categories")) {
			ps->state = ST_CATEGORIES;
			ps->has_categories = true;
		}
		else
			ps->state = ST_INVALID;
		break;
	case ST_CATEGORIES:
		if (!xstricmp(name, "category")) {
			ps->state = ST_CAT;
		}
		else
			ps->state = ST_INVALID;
		break;
	case ST_CAT:
		if (!ps->cat.name && !xstricmp(name, "name")) {
			ps->state = ST_CAT_NAME;
		}
		else
			ps->state = ST_INVALID;
		break;
	case ST_PROGRAMS:
		if (!xstricmp(name, "program")) {
			ps->state = ST_PROG;
			ps->prog = repository_add(ps->repo, ps->perm);
		}
		else
			ps->state = ST_INVALID;
		break;
	case ST_PROG:
		if (!ps->prog->name && !xstricmp(name, "name"))
			ps->state = ST_PROG_NAME;
		else if (!ps->prog->category && !xstricmp(name, "category"))
			ps->state = ST_PROG_CAT;
		else if (!ps->prog->cmd && !xstricmp(name, "commands"))
			ps->state = ST_CMDS;
		else
			ps->state = ST_INVALID;
		break;
	case ST_CMDS:
		if (!xstricmp(name, "command")) {
			ps->state = ST_CMD;
			ps->cmd = arena_new(ps->perm, union command);
		}
		else
			ps->state = ST_INVALID;
		break;
	case ST_CMD:
		if (!xstricmp(name, "arch"))
			ps->state = ST_CMD_ARCH;
		else if (!xstricmp(name, "os"))
			ps->state = ST_CMD_OS;
		else if (!ps->cmd->type && !xstricmp(name, "alert"))
			ps->cmd->type = CMD_ALERT, ps->state = ST_CMD_ALERT;
		else if (!ps->cmd->type && !xstricmp(name, "fail"))
			ps->cmd->type = CMD_FAIL, ps->state = ST_CMD_FAIL;
		else if (!ps->cmd->type && !xstricmp(name, "mkdir"))
			ps->cmd->type = CMD_MKDIR, ps->state = ST_CMD_MKDIR;
		else if (!ps->cmd->type && !xstricmp(name, "cpdir"))
			ps->cmd->type = CMD_CPDIR, ps->state = ST_CMD_CPDIR;
		else if (!ps->cmd->type && !xstricmp(name, "rmdir"))
			ps->cmd->type = CMD_RMDIR, ps->state = ST_CMD_RMDIR;
		else if (!ps->cmd->type && !xstricmp(name, "cpfile"))
			ps->cmd->type = CMD_CPFILE, ps->state = ST_CMD_CPFILE;
		else if (!ps->cmd->type && !xstricmp(name, "rmfile"))
			ps->cmd->type = CMD_RMFILE, ps->state = ST_CMD_RMFILE;
		else if (!ps->cmd->type && !xstricmp(name, "move"))
			ps->cmd->type = CMD_MOVE, ps->state = ST_CMD_MOVE;
		else if (!ps->cmd->type && !xstricmp(name, "rename"))
			ps->cmd->type = CMD_RENAME, ps->state = ST_CMD_RENAME;
		else if (!ps->cmd->type && !xstricmp(name, "cmd"))
			ps->cmd->type = CMD_CMD, ps->state = ST_CMD_CMD;
		else if (!ps->cmd->type && !xstricmp(name, "exec"))
			ps->cmd->type = CMD_EXEC, ps->state = ST_CMD_EXEC;
		else
			ps->state = ST_INVALID;
		break;
	case ST_CMD_CPDIR:
	case ST_CMD_CPFILE:
	case ST_CMD_MOVE:
		if (!ps->cmd->arg1 && !xstricmp(name, "from")) {
			switch (ps->state) {
			case ST_CMD_CPDIR: ps->state = ST_CMD_CPDIR_FROM; break;
			case ST_CMD_CPFILE: ps->state = ST_CMD_CPFILE_FROM; break;
			case ST_CMD_MOVE: ps->state = ST_CMD_MOVE_FROM; break;
			}
		}
		else if (!ps->cmd->arg2 && !xstricmp(name, "to")) {
			switch (ps->state) {
			case ST_CMD_CPDIR: ps->state = ST_CMD_CPDIR_TO; break;
			case ST_CMD_CPFILE: ps->state = ST_CMD_CPFILE_TO; break;
			case ST_CMD_MOVE: ps->state = ST_CMD_MOVE_TO; break;
			}
		}
		else {
			ps->state = ST_INVALID;
			break;
		}
		break;
	case ST_CMD_RENAME:
		if (!ps->cmd->arg1 && !xstricmp(name, "path"))
			ps->state = ST_CMD_RENAME_PATH;
		else if (!ps->cmd->arg2 && !xstricmp(name, "new-name")) 
			ps->state = ST_CMD_RENAME_NEW_NAME;
		else
			ps->state = ST_INVALID;
		break;
	case ST_CMD_EXEC:
		if (!ps->cmd->arg1 && !xstricmp(name, "file"))
			ps->state = ST_CMD_EXEC_FILE;
		else if (!ps->cmd->arg2 && !xstricmp(name, "args"))
			ps->state = ST_CMD_EXEC_ARGS;
		else
			ps->state = ST_INVALID;
		break;
	default:
		ps->state = ST_INVALID;
		break;
	}
}

void XMLCALL on_element_end(struct parser_state *ps,
		const XML_Char *name)
{
	switch (ps->state) {
	case ST_CMD_EXEC_FILE:
	case ST_CMD_EXEC_ARGS:
		ps->state = ST_CMD_EXEC;
		break;
	case ST_CMD_RENAME_PATH:
	case ST_CMD_RENAME_NEW_NAME:
		ps->state = ST_CMD_RENAME;
		break;
	case ST_CMD_CPDIR_FROM:
	case ST_CMD_CPDIR_TO:
		ps->state = ST_CMD_CPDIR;
		break;
	case ST_CMD_CPFILE_FROM:
	case ST_CMD_CPFILE_TO:
		ps->state = ST_CMD_CPFILE;
		break;
	case ST_CMD_MOVE_FROM:
	case ST_CMD_MOVE_TO:
		ps->state = ST_CMD_MOVE;
		break;
	case ST_CMD_EXEC:
	case ST_CMD_CMD:
	case ST_CMD_CPDIR:
	case ST_CMD_CPFILE:
	case ST_CMD_MOVE:
	case ST_CMD_RENAME:
	case ST_CMD_MKDIR:
	case ST_CMD_RMDIR:
	case ST_CMD_ALERT:
	case ST_CMD_FAIL:
	case ST_CMD_OS:
	case ST_CMD_ARCH:
		switch (ps->cmd->type) {
		case CMD_EXEC:
			if (!ps->cmd->arg1 && !ps->cmd->arg2) {
				ps->state = ST_INVALID;
				return;
			}
			break;
		case CMD_CPFILE:
		case CMD_CPDIR:
		case CMD_MOVE:
		case CMD_RENAME:	
			if (!ps->cmd->arg1 || !ps->cmd->arg2) {
				ps->state = ST_INVALID;
				return;
			}
		}
		ps->state = ST_CMD;
		break;
	case ST_CMD:
		if (!ps->cmd_head)
			ps->cmd_tail = ps->cmd_head = ps->cmd;
		else 
			ps->cmd_tail->next = ps->cmd, 
				ps->cmd_tail = ps->cmd_tail->next;
		ps->state = ST_CMDS;
		break;
	case ST_CMDS:
		ps->prog->cmd = ps->cmd_head;
	case ST_PROG_NAME:
	case ST_PROG_CAT:
		ps->state = ST_PROG;
		break;
	case ST_PROG:
		ps->state = ST_PROGRAMS;
		ps->cmd = NULL;
		ps->cmd_head = NULL;
		ps->cmd_tail = NULL;
		ps->prog = NULL;
		break;
	case ST_PROGRAMS:
		ps->state = ST_REPO;
		break;
	case ST_CAT_NAME:
		ps->state = ST_CAT;
		break;
	case ST_CAT:
		repository_add_category(ps->repo, &ps->cat, ps->perm);
		ps->cat.name = NULL;
		ps->state = ST_CATEGORIES;
		break;
	case ST_CATEGORIES:
		ps->state = ST_REPO;
		break;
	case ST_REPO:
		ps->state = ST_END;
		break;
	default:
		ps->state = ST_INVALID;
		break;
	}
}

void XMLCALL on_char_data(struct parser_state *ps,
		const XML_Char *s,
		int len)
{
	struct arena old_perm = *ps->perm;
	char *string;

	switch (ps->state) {
	case ST_CMD_ARCH:
		// parse arch
		if (!xstrnicmp(s, "X86", len))
			ps->cmd->arch |= ARCH_X86;
		else if (!xstrnicmp(s, "X64", len))
			// For Windows 11 ARM64
			ps->cmd->arch |= ARCH_X64 | ARCH_ARM64;
		else if (!xstrnicmp(s, "X86_64", len))
			ps->cmd->arch |= ARCH_X64;
		else if (!xstrnicmp(s, "X86-64", len))
			ps->cmd->arch |= ARCH_X64;
		else if (!xstrnicmp(s, "AMD64", len))
			ps->cmd->arch |= ARCH_X64;
		else if (!xstrnicmp(s, "ARM64", len))
			ps->cmd->arch |= ARCH_ARM64;
		else
			ps->state = ST_INVALID;
		break;
	case ST_CMD_OS:
		if (!xstrnicmp(s, "XP", len) || !xstrnicmp(s, "WinXP", len))
			ps->cmd->os |= OS_WINXP;
		else if (!xstrnicmp(s, "Vista", len) || !xstrnicmp(s, "WinVista", len))
			ps->cmd->os |= OS_WINVISTA;
		else if (!xstrnicmp(s, "7", len) || !xstrnicmp(s, "Win7", len))
			ps->cmd->os |= OS_WIN_7;
		else if (!xstrnicmp(s, "8", len) || !xstrnicmp(s, "Win8", len))
			ps->cmd->os |= OS_WIN_8;
		else if (!xstrnicmp(s, "8.1", len) || !xstrnicmp(s, "Win8.1", len))
			ps->cmd->os |= OS_WIN_8_1;
		else if (!xstrnicmp(s, "10", len) || !xstrnicmp(s, "Win10", len))
			ps->cmd->os |= OS_WIN_10;
		else if (!xstrnicmp(s, "11", len) || !xstrnicmp(s, "Win11", len))
			ps->cmd->os |= OS_WIN_11;
		else
			ps->state = ST_INVALID;
		break;
	default:
		string = arena_new(ps->perm, char, len + 1);
		memcpy(string, s, len);
		break;
	}

	switch (ps->state) {
	case ST_CAT_NAME:        ps->cat.name         = string; break;
	case ST_PROG_NAME:       ps->prog->name       = string; break;
	case ST_PROG_CAT:        ps->prog->category   = string; break;
	case ST_CMD_MKDIR:       ps->cmd->mkdir.path  = string; break;
	case ST_CMD_RMDIR:       ps->cmd->rmdir.path  = string; break;
	case ST_CMD_RMFILE:      ps->cmd->rmfile.path = string; break;
	case ST_CMD_CMD:         ps->cmd->cmd.line    = string; break;
	case ST_CMD_ALERT:       ps->cmd->alert.msg   = string; break;
	case ST_CMD_FAIL:        ps->cmd->fail.msg    = string; break;
	case ST_CMD_EXEC_FILE:   ps->cmd->exec.path   = string; break;
	case ST_CMD_EXEC_ARGS:   ps->cmd->exec.args   = string; break;
	case ST_CMD_MOVE_FROM:   ps->cmd->move.from   = string; break;
	case ST_CMD_MOVE_TO:     ps->cmd->move.to     = string; break;
	case ST_CMD_RENAME_PATH: ps->cmd->rename.path = string; break;
	case ST_CMD_RENAME_NEW_NAME: ps->cmd->rename.newname = string; break;
	case ST_CMD_CPFILE_FROM: ps->cmd->cpfile.from = string; break;
	case ST_CMD_CPFILE_TO:   ps->cmd->cpfile.to   = string; break;
	case ST_CMD_CPDIR_FROM:  ps->cmd->cpdir.from  = string; break;
	case ST_CMD_CPDIR_TO:    ps->cmd->cpdir.to    = string; break;
	case ST_CMD_ARCH:
	case ST_CMD_OS:
		break;
	default:
		*ps->perm = old_perm;
		break;
	}
}
