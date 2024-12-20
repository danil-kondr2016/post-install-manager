#define UNICODE  1
#define _UNICODE 1

#include "resource.h"
#include "install.h"
#include "commands.h"
#include "fatal.h"
#include "errors.h"

#ifndef PSH_AEROWIZARD
#define PSH_AEROWIZARD 0x4000
#endif

#include <string.h>
#include <wchar.h>

#include <commctrl.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <strsafe.h>

#define IPM_DONE (WM_APP + 1)
#define IPM_ERROR (WM_APP + 2)

static HIMAGELIST create_imglist_checkboxes(HWND hWnd);
static void load_repository(struct installer *installer);
static void mark_programs(struct installer *installer);
static INT_PTR CALLBACK select_page_proc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK install_page_proc(HWND, UINT, WPARAM, LPARAM);
static DWORD WINAPI installer_thread(struct installer *installer);

uint32_t run_installer(struct installer *installer, struct arena *perm, 
		struct arena scratch)
{
	uint32_t result = 0;

	if (!installer)
		return STATUS_INVALID_PARAMETER;

	if (!GetModuleHandleA("ntdll.dll"))
		LoadLibraryA("ntdll.dll");

	result = repository_parse(&installer->repo, "pim.xml", perm, scratch);
	if (NT_ERROR(result))
		return result;

	installer->psp[0].dwSize = sizeof(PROPSHEETPAGEW);
	installer->psp[0].dwFlags = PSP_USEHEADERTITLE;
	installer->psp[0].hInstance = installer->instance;
	installer->psp[0].pszHeaderTitle = L"Выбор программ";
	installer->psp[0].pszTemplate = L"IDD_SELECT";
	installer->psp[0].pfnDlgProc = select_page_proc;
	installer->psp[0].lParam = (LPARAM)installer;

	installer->psp[1].dwSize = sizeof(PROPSHEETPAGEW);
	installer->psp[1].dwFlags = PSP_USEHEADERTITLE;
	installer->psp[1].hInstance = installer->instance;
	installer->psp[1].pszHeaderTitle = L"Установка программ";
	installer->psp[1].pfnDlgProc = install_page_proc;
	installer->psp[1].pszTemplate = L"IDD_PROCESS";
	installer->psp[1].lParam = (LPARAM)installer;

	installer->psh.dwSize = sizeof(PROPSHEETHEADERW);
	installer->psh.dwFlags = PSH_WIZARD97 | PSH_PROPSHEETPAGE;
	installer->psh.nPages = 2;
	installer->psh.ppsp = installer->psp;

	PropertySheetW(&installer->psh);
	return 0;
}

static INT_PTR CALLBACK select_page_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PROPSHEETPAGE *page;
	struct installer *installer;
	LPNMHDR lpnmhdr;

	switch (msg) {
	case WM_INITDIALOG:
		page = (PROPSHEETPAGE *)lParam;
		installer = (struct installer *)page->lParam;
		SetWindowLongPtrW(hWnd, DWLP_USER, (LONG_PTR)installer);
		installer->imglist = create_imglist_checkboxes(hWnd);
		installer->software_list = GetDlgItem(hWnd, IDC_PROGLIST);
		ListView_SetExtendedListViewStyle(installer->software_list, LVS_EX_CHECKBOXES);
		ListView_SetImageList(installer->software_list, installer->imglist, LVSIL_STATE);
		load_repository(installer);
		break;
	case WM_NOTIFY:
		lpnmhdr = (LPNMHDR)lParam;
		switch (lpnmhdr->code) {
		case PSN_WIZNEXT:
			installer = (struct installer *)GetWindowLongPtrW(hWnd, DWLP_USER);
			mark_programs(installer);
			PropSheet_SetWizButtons(GetParent(hWnd), 0);
			break;
		case PSN_SETACTIVE:
			PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_NEXT);
			break;
		}
		break;
	}	
	return FALSE;
}

static INT_PTR CALLBACK install_page_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PROPSHEETPAGE *page;
	struct installer *installer;
	LPNMHDR lpnmhdr;
	DWORD result;	
	WCHAR buffer[1024];

	switch (msg) {
	case WM_INITDIALOG:
		page = (PROPSHEETPAGE *)lParam;
		installer = (struct installer *)page->lParam;
		SetWindowLongPtrW(hWnd, DWLP_USER, (LONG_PTR)installer);
		installer->progress_bar = GetDlgItem(hWnd, IDC_PROGRESS);
		installer->install_page = hWnd;
		SendMessageW(installer->progress_bar, PBM_SETRANGE32, 0, installer->cmd_count);
		installer->command_memo = GetDlgItem(hWnd, IDC_COMMANDS);
		installer->installed_software = GetDlgItem(hWnd, IDC_INSTALLING);
		installer->thread = CreateThread(NULL, 65536, (LPTHREAD_START_ROUTINE)installer_thread,
				(LPVOID)installer, 0, NULL);
		break;
	case WM_NOTIFY:
		lpnmhdr = (LPNMHDR)lParam;
		switch (lpnmhdr->code) {
		case PSN_SETACTIVE:
			EnableWindow(GetDlgItem(GetParent(hWnd), IDCANCEL), FALSE);
			PropSheet_SetWizButtons(GetParent(hWnd), 0);
			break;
		}
		break;
	case IPM_DONE:
		installer = (struct installer *)GetWindowLongPtrW(hWnd, DWLP_USER);
		LoadStringW(installer->instance, IDS_COMPLETE_ALL, buffer, 1024);
		PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_FINISH);
		SetDlgItemTextW(hWnd, IDC_STATUS, buffer);
		SetDlgItemTextW(hWnd, IDC_INSTALLING, L"");
		MessageBoxW(hWnd, L"Установка завершена.", L"", MB_ICONINFORMATION);
		break;
	case IPM_ERROR:
		installer = (struct installer *)GetWindowLongPtrW(hWnd, DWLP_USER);
		result = lParam;
		CloseHandle(installer->thread);
		PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_FINISH);
		LoadStringW(installer->instance, IDS_ERROR, buffer, 1024);
		SetDlgItemTextW(hWnd, IDC_STATUS, buffer);
		SetDlgItemTextW(hWnd, IDC_INSTALLING, L"");

		if (result == STATUS_UNSUCCESSFUL) {
			MessageBoxW(hWnd, buffer, L"", MB_ICONHAND);
		}
		else if (result != PIM_ERROR_FAIL) {
			error_msgW(hWnd, result);
		}
		break;
	}	
	return FALSE;

}

static void load_repository(struct installer *installer)
{
	LVCOLUMNW column = {0};
	LVITEMW item = {0};
	int i = 0;

	column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	column.pszText = L"Название";
	column.cx = 200;
	SendMessageW(installer->software_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&column);
	ListView_SetColumnWidth(installer->software_list, 1, LVSCW_AUTOSIZE);

	item.mask = LVIF_TEXT | LVIF_PARAM;
	for (struct program *prog = installer->repo.head; prog; prog = prog->next, i++) {
		struct arena scratch = installer->scratch;
		item.pszText = u8_to_u16(prog->name, &scratch);
		item.lParam = (LPARAM)prog;
		item.iItem = i;

		SendMessageW(installer->software_list, LVM_INSERTITEMW, 0, (LPARAM)&item);
		ListView_SetItemState(installer->software_list, i, 
				INDEXTOSTATEIMAGEMASK(2),
				LVIS_STATEIMAGEMASK);
	}
}

static void mark_programs(struct installer *installer)
{
	int item_count = ListView_GetItemCount(installer->software_list);
	LVITEM item = {0};

	item.mask = LVIF_PARAM;
	for (int i = 0; i < item_count; i++) {
		item.iItem = i;
		ListView_GetItem(installer->software_list, &item);

		if (!ListView_GetCheckState(installer->software_list, i))
			repository_delete(&installer->repo, (struct program *)item.lParam);
	}

	installer->cmd_count = 0;
	for (struct program *prog = installer->repo.head;
			prog;
			prog = prog->next)
	{
		for (union command *cmd = prog->cmd; cmd; cmd = cmd->next)
			installer->cmd_count++;
	}
}

static HIMAGELIST create_imglist_checkboxes(HWND hWnd)
{
	RECT full = {0, 0, 48, 16};
	static struct { int stateId; unsigned state; } data[] = {
		{CBS_UNCHECKEDNORMAL, DFCS_BUTTONCHECK | DFCS_FLAT},
		{CBS_CHECKEDNORMAL,   DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_FLAT},
		{-1,0},
	};

	HTHEME theme = OpenThemeData(hWnd, L"BUTTON");
	HDC dc = GetDC(hWnd);
	if (!dc)
		goto cleanup1;

	HBITMAP bmpCheckboxes = CreateCompatibleBitmap(dc, 32, 16);
	if (!bmpCheckboxes)
		goto cleanup2;

	HIMAGELIST imglist = ImageList_Create(16, 16, ILC_COLOR32, 2, 1);
	if (!imglist)
		goto cleanup2;

	HDC dcMem = CreateCompatibleDC(dc);
	if (!dcMem) {
		ImageList_Destroy(imglist);
		imglist = NULL;
		goto cleanup2;
	}

	HBITMAP oldBmp = SelectObject(dcMem, bmpCheckboxes);
	FillRect(dcMem, &full, GetStockObject(WHITE_BRUSH));

	RECT image = {0, 0, 16, 16};
	for (int i = 0; data[i].stateId >= 0; i++) {
		if (theme) {
			DrawThemeBackground(theme, dcMem, BP_CHECKBOX, data[i].stateId, &image, NULL);
			DrawThemeEdge(theme, dcMem, BP_CHECKBOX, data[i].stateId, &image, 0, 0, NULL);
		}
		else {
			DrawFrameControl(dcMem, &image, DFC_BUTTON, data[i].state);
		}

		image.left += 16;
		image.right += 16;
	}

	SelectObject(dcMem, oldBmp);
	ImageList_Add(imglist, bmpCheckboxes, 0);

cleanup3: DeleteDC(dcMem);
cleanup2: ReleaseDC(hWnd, dc);
cleanup1: if (theme)
		  CloseThemeData(theme);
exit_fn:  return imglist;
}

static wchar_t *load_string_resource(HINSTANCE instance, UINT resource, struct arena *arena)
{
	size_t size;
	wchar_t *result;

	size = LoadStringW(instance, resource, (LPWSTR)&result, 0);
	result = arena_new(arena, wchar_t, size + 1);
	LoadStringW(instance, resource, result, size + 1);

	return result;
}

static void format_cmd(struct installer *installer, union command *cmd)
{
	if (cmd->type >= CMD_Count)
		return;

	struct arena scratch = installer->scratch;
	wchar_t *fmt_str = load_string_resource(installer->instance, IDS_CMD_NULL+cmd->type, &scratch);
	char *arg1, *arg2 = NULL;

	switch (cmd->type) {
	case CMD_NULL:
	case CMD_ALERT:
	case CMD_FAIL:
		SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)fmt_str);
		return;
	case CMD_CMD: arg1 = cmd->cmd.line; break;
	case CMD_MKDIR: arg1 = cmd->mkdir.path; break;
	case CMD_RMDIR: arg1 = cmd->rmdir.path; break;
	case CMD_RMFILE: arg1 = cmd->rmfile.path; break;
	case CMD_EXEC:
		arg1 = cmd->exec.path;
		arg2 = cmd->exec.args;
		break;
	case CMD_CPDIR:
		arg1 = cmd->cpdir.from;
		arg2 = cmd->cpdir.to;
		break;
	case CMD_CPFILE:
		arg1 = cmd->cpfile.from;
		arg2 = cmd->cpfile.to;
		break;
	case CMD_MOVE:
		arg1 = cmd->move.from;
		arg2 = cmd->move.to;
		break;
	case CMD_RENAME:
		arg1 = cmd->rename.path;
		arg2 = cmd->rename.newname;
		break;
	}

	wchar_t *arg1W = u8_to_u16(arg1, &scratch);
	wchar_t *arg2W = u8_to_u16(arg2, &scratch);
	size_t formatted_size = wcslen(arg1W) + wcslen(arg2W) + wcslen(fmt_str) + 1;
	wchar_t *formatted = arena_new(&scratch, wchar_t, formatted_size);
	switch (cmd->type) {
	case CMD_CMD:
	case CMD_MKDIR:
	case CMD_RMDIR:
	case CMD_RMFILE:
		StringCchPrintfW(formatted, formatted_size, fmt_str, arg1W);
		break;
	case CMD_EXEC:
	case CMD_CPDIR:
	case CMD_CPFILE:
	case CMD_MOVE:
	case CMD_RENAME:
		StringCchPrintfW(formatted, formatted_size, fmt_str, arg1W, arg2W);
		break;
	}

	SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)formatted);
}

static DWORD WINAPI installer_thread(struct installer *installer)
{
	DWORD result = 0;
	DWORD pos;
	struct arena old_scratch = installer->scratch;
	wchar_t *prog_text;
	size_t prog_text_size;
	wchar_t *complete, *error;

	prog_text_size = LoadStringW(installer->instance, IDS_INSTALLING, (LPWSTR)&prog_text, 0);
	prog_text = arena_new(&installer->scratch, wchar_t, prog_text_size + 1);
	LoadStringW(installer->instance, IDS_INSTALLING, prog_text, prog_text_size + 1);
	complete = load_string_resource(installer->instance, IDS_COMPLETE, &installer->scratch);
	error = load_string_resource(installer->instance, IDS_ERROR, &installer->scratch);
	for (struct program *prog = installer->repo.head;
			prog;
			prog = prog->next)
	{
		struct arena scratch = installer->scratch;

		wchar_t *prog_nameW = u8_to_u16(prog->name, &scratch);
		size_t prog_buf_count = prog_text_size + wcslen(prog_nameW) + 1;
		wchar_t *prog_buf = arena_new(&scratch, wchar_t, prog_buf_count);
		StringCchPrintfW(prog_buf, prog_buf_count, prog_text, prog_nameW);
		SetWindowTextW(installer->installed_software, prog_buf);
		SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)prog_buf);
		
		for (union command *cmd = prog->cmd; cmd; cmd = cmd->next) {
			format_cmd(installer, cmd);
			result = execute_command(prog->cmd, scratch);

			if (result != 0) {
				result = STATUS_UNSUCCESSFUL;	
			}

			if (NT_ERROR(result)) {
				SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)error);
				SendMessageW(installer->progress_bar, PBM_SETSTATE, PBST_ERROR, 0);
				PostMessageW(installer->install_page, IPM_ERROR, 0, result);
				return result;
			}

			pos = SendMessageW(installer->progress_bar, PBM_GETPOS, 0, 0);
			SendMessageW(installer->progress_bar, PBM_SETPOS, pos+1, 0);
		}	

		SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)complete);
		SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)L"");
	}

	if (result == PIM_STATUS_SKIPPED) result = STATUS_SUCCESS;

	PostMessageW(installer->install_page, IPM_DONE, 0, 0);

	installer->scratch = old_scratch;
	return result;
}
