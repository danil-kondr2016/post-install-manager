#include "commands.h"
#include "fileops.h"

#include <windows.h>

uint32_t execute_command_chain(union command *cmd, struct arena scratch)
{
	uint32_t result = 0;

	for (union command *c = cmd; c; c = c->next) {
		result = execute_command(cmd, scratch);
		if ((result & 0xC0000000) == 0xC0000000)
			break;
	}

	if (result == 0x27F10000)
		result = 0x00000000;
	return result;
}

static uint32_t execute(char *path, char *args, struct arena scratch);
static uint32_t command(char *line, struct arena scratch);
static uint32_t alert(char *msg, struct arena scratch);
static uint32_t fail(char *msg, struct arena scratch);

uint32_t execute_command(union command *cmd, struct arena scratch)
{
	uint32_t result = 0;

	if (!cmd)
		return 0x00000001;

	switch (cmd->type) {
	case CMD_EXEC:   result = execute(cmd->exec.path, cmd->exec.args, scratch); break;
	case CMD_CMD:    result = command(cmd->cmd.line, scratch); break;
	case CMD_MKDIR:  result = op_mkdir(cmd->mkdir.path, scratch); break;
	case CMD_RMDIR:  result = op_remove_dir(cmd->rmdir.path, scratch); break;
	case CMD_RMFILE: result = op_remove_file(cmd->rmfile.path, scratch); break;
	case CMD_CPDIR:  result = op_copy_dir(cmd->cpdir.from, cmd->cpdir.to, scratch); break;
	case CMD_CPFILE: result = op_copy_dir(cmd->cpfile.from, cmd->cpfile.to, scratch); break;
	case CMD_MOVE:   result = op_move(cmd->move.from, cmd->move.to, scratch); break;
	case CMD_RENAME: result = op_move(cmd->rename.path, cmd->rename.newname, scratch); break;
	case CMD_ALERT:  result = alert(cmd->alert.msg, scratch); break;
	case CMD_FAIL:   result = fail(cmd->fail.msg, scratch); break;
	default:         result = 0x00000001;
	}

	return result;
}

static uint32_t alert(char *msg, struct arena scratch)
{
	wchar_t *msgW = u8_to_u16(msg, &scratch);
	MessageBoxW(NULL, msgW, NULL, MB_ICONEXCLAMATION | MB_OK);
	return 0x00000000;
}

static uint32_t fail(char *msg, struct arena scratch)
{
	wchar_t *msgW = u8_to_u16(msg, &scratch);
	MessageBoxW(NULL, msgW, NULL, MB_ICONEXCLAMATION | MB_OK);
	return 0xC0000001;
}

static uint32_t execute(char *path, char *args, struct arena scratch)
{
	DWORD result = 0;
	SHELLEXECUTEINFOW sei = {0};

	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS 
		| SEE_MASK_DOENVSUBST 
		| SEE_MASK_FLAG_NO_UI;
	sei.lpVerb = L"open";
	sei.lpFile = u8_to_u16(path, &scratch);
	sei.lpParameters = u8_to_u16(args, &scratch);
	sei.nShow = SW_NORMAL;
	
	if (!ShellExecuteExW(&sei)) {
		result = 0xC0070000 | (GetLastError() & 0xFFFF);
	}
	else {
		if (sei.hProcess) {
			WaitForSingleObject(sei.hProcess, INFINITE);
			GetExitCodeProcess(sei.hProcess, &result);
			CloseHandle(sei.hProcess);
		}
		else {
			result = 0xC0000008;
		}
	}

	return result;
}

static uint32_t command(char *line, struct arena scratch)
{
	wchar_t cmdPathW_unexp[] = L"%SystemRoot%\\System32\\cmd.exe";
	wchar_t *lineW = arena_new(&scratch, wchar_t, 3);
	size_t cmdPathW_size;
	wchar_t *cmdPathW;
	DWORD result = 0;
	STARTUPINFOW si = {0};
	PROCESS_INFORMATION pi = {0};

	lineW[0] = '/';
	lineW[1] = 'c';
	lineW[2] = ' ';
	
	u8_to_u16(line, &scratch);

	cmdPathW_size = ExpandEnvironmentStringsW(cmdPathW_unexp, NULL, 0);
	cmdPathW = arena_new(&scratch, wchar_t, cmdPathW_size + 1);
	ExpandEnvironmentStringsW(cmdPathW_unexp, cmdPathW, cmdPathW_size + 1);

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	if (!CreateProcessW(cmdPathW, lineW, NULL, NULL, FALSE,
				CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
		result = 0xC0070000 | (GetLastError() & 0xFFFF);		
	}
	else {
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &result);
		CloseHandle(pi.hProcess);
	}

	return result;
}
