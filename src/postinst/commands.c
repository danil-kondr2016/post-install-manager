#include "commands.h"
#include "fileops.h"
#include "errors.h"

#include <windows.h>
#include <winternl.h>
#include <stdbool.h>

uint32_t execute_command_chain(union command *cmd, struct arena scratch)
{
	uint32_t result = 0;

	for (union command *c = cmd; c; c = c->next) {
		result = execute_command(cmd, scratch);
		if (NT_ERROR(result))
			break;
	}

	if (result == PIM_STATUS_SKIPPED)
		result = STATUS_SUCCESS;
	return result;
}

static bool test_arch(uint32_t flags);
static bool test_os(uint32_t flags);

static uint32_t execute(char *path, char *args, struct arena scratch);
static uint32_t command(char *line, struct arena scratch);
static uint32_t alert(char *msg, struct arena scratch);
static uint32_t fail(char *msg, struct arena scratch);

uint32_t execute_command(union command *cmd, struct arena scratch)
{
	uint32_t result = 0;

	if (!cmd)
		return STATUS_UNSUCCESSFUL;
	if (!test_arch(cmd->arch))
		return PIM_STATUS_SKIPPED;
	if (!test_os(cmd->os))
		return PIM_STATUS_SKIPPED;

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
	default:         result = STATUS_UNSUCCESSFUL;
	}

	return result;
}

static uint32_t alert(char *msg, struct arena scratch)
{
	wchar_t *msgW = u8_to_u16(msg, &scratch);
	MessageBoxW(NULL, msgW, L"", MB_ICONEXCLAMATION | MB_OK);
	return STATUS_SUCCESS;
}

static uint32_t fail(char *msg, struct arena scratch)
{
	wchar_t *msgW = u8_to_u16(msg, &scratch);
	MessageBoxW(NULL, msgW, NULL, MB_ICONHAND | MB_OK);
	return PIM_ERROR_FAIL;
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
		result = NTSTATUS_FROM_WIN32(GetLastError());
	}
	else {
		if (sei.hProcess) {
			WaitForSingleObject(sei.hProcess, INFINITE);
			GetExitCodeProcess(sei.hProcess, &result);
			CloseHandle(sei.hProcess);
		}
		else {
			result = STATUS_INVALID_HANDLE;
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
		result = NTSTATUS_FROM_WIN32(GetLastError());		
	}
	else {
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &result);
		CloseHandle(pi.hProcess);
	}

	return result;
}

static bool test_arch(uint32_t flags)
{
	SYSTEM_INFO info;

	if (!flags)
		return true;
	GetNativeSystemInfo(&info);

	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		return (flags & ARCH_X86) != 0;
	case PROCESSOR_ARCHITECTURE_AMD64:
		return (flags & ARCH_X64) != 0;
	case PROCESSOR_ARCHITECTURE_ARM64:
		return (flags & ARCH_ARM64) != 0;
	default:
		return false;
	}
}

typedef NTSTATUS (*P_RtlGetVersion)(POSVERSIONINFOW lpVI);

static bool test_os(uint32_t flags)
{
	OSVERSIONINFOW osi = {0};
	P_RtlGetVersion RtlGetVersion = NULL;
	HMODULE hNtdll;

	if (!flags)
		return true;

	hNtdll = GetModuleHandleA("ntdll.dll");
	RtlGetVersion = (P_RtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
	if (NULL == RtlGetVersion)
		return false;

	osi.dwOSVersionInfoSize = sizeof(osi);
	RtlGetVersion(&osi);	

	if (osi.dwMajorVersion == 10 && osi.dwMinorVersion == 0) {
		if (osi.dwBuildNumber >= 22000)
			return (flags & OS_WIN_11) != 0;
		else if (osi.dwBuildNumber >= 10240)
			return (flags & OS_WIN_10) != 0;
	}
	else if (osi.dwMajorVersion == 6) {
		switch (osi.dwMinorVersion) {
		case 3: return (flags & OS_WIN_8_1) != 0;
		case 2: return (flags & OS_WIN_8) != 0;
		case 1: return (flags & OS_WIN_7) != 0;
		case 0: return (flags & OS_WINVISTA) != 0;
		}
	}
	else if (osi.dwMajorVersion == 5 && osi.dwMinorVersion == 1) {
		return (flags & OS_WINXP) != 0;
	}

	return false; // some kind of unrecognized or beta version of OS
}
