#pragma once

#include "arena.h"

union command;

enum {
	ARCH_X86    = UINT32_C(0x00000001),
	ARCH_X64    = UINT32_C(0x00000002),
	ARCH_ARM64  = UINT32_C(0x00000004),

	OS_WINXP    = UINT32_C(0x00000001),
	OS_WINVISTA = UINT32_C(0x00000002),
	OS_WIN_7    = UINT32_C(0x00000004),
	OS_WIN_8    = UINT32_C(0x00000008),
	OS_WIN_8_1  = UINT32_C(0x00000010),
	OS_WIN_10   = UINT32_C(0x00000020),
	OS_WIN_11   = UINT32_C(0x00000040),
};

#define Cmd_Header     \
	intptr_t type; \
	uint32_t arch; \
	uint32_t os;   \
	union command *next;

struct cmd_header {
	Cmd_Header
};

struct cmd_exec {
	Cmd_Header
	char *path, *args;
};

struct cmd_fail {
	Cmd_Header
	char *msg;
};

struct cmd_alert {
	Cmd_Header
	char *msg;
};

struct cmd_cmd {
	Cmd_Header
	char *line;
};

struct cmd_cpdir {
	Cmd_Header
	char *from, *to;
};

struct cmd_cpfile {
	Cmd_Header
	char *from, *to;
};

struct cmd_move {
	Cmd_Header
	char *from, *to;
};

struct cmd_rename {
	Cmd_Header
	char *path, *newname;
};

struct cmd_mkdir {
	Cmd_Header
	char *path;
};

struct cmd_rmdir {
	Cmd_Header
	char *path;
};

struct cmd_rmfile {
	Cmd_Header
	char *path;
};

union command {
	struct { Cmd_Header };
	struct cmd_exec   exec;
	struct cmd_cmd    cmd;
	struct cmd_fail   fail;
	struct cmd_alert  alert;
	struct cmd_cpdir  cpdir;
	struct cmd_mkdir  mkdir;
	struct cmd_rmdir  rmdir;
	struct cmd_cpfile cpfile;
	struct cmd_rmfile rmfile;
	struct cmd_move   move;
	struct cmd_rename rename;
};

enum {
	CMD_NULL,
	CMD_EXEC,
	CMD_CMD,
	CMD_ALERT,
	CMD_FAIL,
	CMD_CPDIR,
	CMD_MKDIR,
	CMD_RMDIR,
	CMD_CPFILE,
	CMD_RMFILE,
	CMD_MOVE,
	CMD_RENAME,
	CMD_Count
};

struct program {
	char *name;
	union command *cmd;
	struct program *next;
};

struct repository {
	struct program *head;
	struct program *tail;
};

struct program *repository_add(struct repository *repo, struct arena *perm);

uint32_t repository_parse(struct repository *repo, char *file_name,
		struct arena *perm,
		struct arena scratch);
