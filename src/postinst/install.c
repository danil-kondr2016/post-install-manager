#define UNICODE  1
#define _UNICODE 1

#include "resource.h"
#include "install.h"
#include "commands.h"
#include "fatal.h"
#include "errors.h"
#include "tools.h"

#ifndef PSH_AEROWIZARD
#define PSH_AEROWIZARD 0x4000
#endif

#include <string.h>
#include <wchar.h>

#include <commctrl.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <strsafe.h>

#include <stdbool.h>

#define IPM_DONE (WM_APP + 1)
#define IPM_ERROR (WM_APP + 2)

static HIMAGELIST create_imglist_checkboxes(HWND hWnd);
static void load_repository(struct installer *installer);
static void mark_programs(struct installer *installer);
static INT_PTR CALLBACK select_page_proc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK install_page_proc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK finish_page_proc(HWND, UINT, WPARAM, LPARAM);
static DWORD WINAPI installer_thread(struct installer *installer);
static void init_pimpath(struct installer *installer, struct arena *perm, 
		struct arena scratch);
static char *expand_pimpath(struct installer *installer, char *ch, 
		struct arena *arena);

uint32_t run_installer(struct installer *installer, struct arena *perm, 
		struct arena scratch)
{
	uint32_t result = 0;

	if (!installer)
		return STATUS_INVALID_PARAMETER;

	if (!GetModuleHandleA("ntdll.dll"))
		LoadLibraryA("ntdll.dll");

	init_pimpath(installer, perm, scratch);
	result = repository_parse(&installer->repo, "pim.xml", perm, scratch);
	if (NT_ERROR(result))
		return result;

	installer->taskbar_button_created = RegisterWindowMessageW(L"TaskbarButtonCreated");

	installer->psp[0].dwSize = sizeof(PROPSHEETPAGEW);
	installer->psp[0].dwFlags = PSP_USEHEADERTITLE;
	installer->psp[0].hInstance = installer->instance;
	installer->psp[0].pszHeaderTitle = MAKEINTRESOURCEW(IDS_SELECT_PROGRAMS);
	installer->psp[0].pszTemplate = L"IDD_SELECT";
	installer->psp[0].pfnDlgProc = select_page_proc;
	installer->psp[0].lParam = (LPARAM)installer;

	installer->psp[1].dwSize = sizeof(PROPSHEETPAGEW);
	installer->psp[1].dwFlags = PSP_USEHEADERTITLE;
	installer->psp[1].hInstance = installer->instance;
	installer->psp[1].pszHeaderTitle = MAKEINTRESOURCEW(IDS_INSTALL_PROGRAMS);
	installer->psp[1].pfnDlgProc = install_page_proc;
	installer->psp[1].pszTemplate = L"IDD_PROCESS";
	installer->psp[1].lParam = (LPARAM)installer;

	installer->psp[2].dwSize = sizeof(PROPSHEETPAGEW);
	installer->psp[2].dwFlags = PSP_DEFAULT | PSP_HIDEHEADER;
	installer->psp[2].hInstance = installer->instance;
	installer->psp[2].pfnDlgProc = finish_page_proc;
	installer->psp[2].pszTemplate = L"IDD_FINISH";
	installer->psp[2].lParam = (LPARAM)installer;

	installer->psp[3].dwSize = sizeof(PROPSHEETPAGEW);
	installer->psp[3].dwFlags = PSP_DEFAULT | PSP_HIDEHEADER;
	installer->psp[3].hInstance = installer->instance;
	installer->psp[3].pfnDlgProc = finish_page_proc;
	installer->psp[3].pszTemplate = L"IDD_FAIL";
	installer->psp[3].lParam = (LPARAM)installer;

	installer->psh.dwSize = sizeof(PROPSHEETHEADERW);
	installer->psh.dwFlags = PSH_WIZARD97 | PSH_PROPSHEETPAGE;
	installer->psh.nPages = 4;
	installer->psh.ppsp = installer->psp;

	PropertySheetW(&installer->psh);
	return 0;
}

static INT_PTR CALLBACK select_page_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PROPSHEETPAGE *page;
	struct installer *installer;
	LPNMHDR lpnmhdr;
	HRESULT hr;

	switch (msg) {
	case WM_INITDIALOG:
		page = (PROPSHEETPAGE *)lParam;
		installer = (struct installer *)page->lParam;
		SetWindowLongPtrW(hWnd, DWLP_USER, (LONG_PTR)installer);
		installer->dialog = GetParent(hWnd);
		hr = CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_ALL, &IID_ITaskbarList3, (void *)&installer->taskbar);
		if (FAILED(hr)) installer->taskbar = NULL;
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
		PropSheet_SetCurSel(GetParent(hWnd), NULL, 2);
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

		PropSheet_SetCurSel(GetParent(hWnd), NULL, 3);
		break;
	}	
	return FALSE;

}

static INT_PTR CALLBACK finish_page_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PROPSHEETPAGE *page;
	struct installer *installer;
	LPNMHDR lpnmhdr;
	WCHAR buffer[1024];
	HFONT font;

	font = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
			RUSSIAN_CHARSET, OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH,
			L"MS Shell Dlg 2");
	if (!font) {
		MessageBoxW(NULL, L"Failed to create new font", L"Error", MB_ICONHAND);
	}

	switch (msg) {
	case WM_INITDIALOG:
		SendDlgItemMessageW(hWnd, IDC_TITLE, WM_SETFONT, (WPARAM)font, TRUE);
		break;
	case WM_NOTIFY:
		lpnmhdr = (LPNMHDR)lParam;
		switch (lpnmhdr->code) {
		case PSN_SETACTIVE:
			PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_FINISH);
			EnableWindow(GetDlgItem(GetParent(hWnd), IDCANCEL), FALSE);
			break;
		}
		break;
	}	
	return FALSE;

}


static void create_group_0(struct installer *installer, struct arena scratch)
{
	WCHAR header[256] = {0};
	LVGROUP group = {0};

	LoadString(installer->instance, IDS_GRP_DEFAULT, header, 256);

	group.cbSize = sizeof(group);
	group.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
	group.state = group.stateMask = LVGS_NORMAL | LVGS_COLLAPSIBLE;
	group.iGroupId = 0;
	group.pszHeader = header;
	group.cchHeader = wcslen(group.pszHeader);
	ListView_InsertGroup(installer->software_list, -1, &group);
}

static void load_groups(struct installer *installer, 
		struct node *cat, struct arena scratch)
{
	LVGROUP group = {0};
	int ret;

	if (!cat)
		return;

	load_groups(installer, cat->child[0], scratch);

	group.cbSize = sizeof(group);
	group.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
	group.state = group.stateMask = LVGS_NORMAL | LVGS_COLLAPSIBLE;
	if (cat->type == NODE_CATEGORY) {
		group.pszHeader = u8_to_u16(cat->cat->name, &scratch);
		group.cchHeader = wcslen(group.pszHeader);
		group.iGroupId = cat->cat->id;

		ret = ListView_InsertGroup(installer->software_list, -1, &group);
	}

	load_groups(installer, cat->child[1], scratch);
}

static void assign_groups(struct installer *installer);
static void load_repository(struct installer *installer)
{
	LVCOLUMNW column = {0};
	LVITEMW item = {0};
	int i = 0;

	if (installer->repo.categories) {
		ListView_EnableGroupView(installer->software_list, TRUE);
		load_groups(installer, installer->repo.categories, installer->scratch);
		create_group_0(installer, installer->scratch);
	}

	WCHAR name_string[256];
	LoadStringW(installer->instance, IDS_NAME, name_string, 256);

	column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	column.pszText = name_string;
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

	if (installer->repo.categories)
		assign_groups(installer);
}

static void assign_groups(struct installer *installer)
{
	int item_count = ListView_GetItemCount(installer->software_list);
	LVITEM item = {0};

	item.mask = LVIF_PARAM | LVIF_GROUPID;
	for (int i = 0; i < item_count; i++) {
		item.iItem = i;
		ListView_GetItem(installer->software_list, &item);

		struct program *prog = (struct program *)item.lParam;		
		struct category *cat = repository_get_category(&installer->repo, prog->category);
		item.iGroupId = cat ? cat->id : 0;
		ListView_SetItem(installer->software_list, &item);
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
		{CBS_UNCHECKEDNORMAL, DFCS_BUTTONCHECK},
		{CBS_CHECKEDNORMAL,   DFCS_BUTTONCHECK | DFCS_CHECKED},
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
	}

	wchar_t *arg1W = u8_to_u16(cmd->arg1, &scratch);
	wchar_t *arg2W = u8_to_u16(cmd->arg2, &scratch);
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

static void init_pimpath(struct installer *installer, struct arena *perm, 
		struct arena scratch)
{
	wchar_t *output = (wchar_t *)scratch.begin;

	size_t size = (scratch.end - scratch.begin) / 2;
	GetModuleFileNameW(NULL, output, size);

	wchar_t *last = wcsrchr(output, '\\');
	if (last) {
		*last = L'\0';
	}

	installer->pimpath = u16_to_u8(output, perm);
}

static char *expand_pimpath(struct installer *installer, char *ch, struct arena *arena)
{
	bool inside_percents = false;
	char *p_begin, *p_end;
	char *p = ch;
	size_t length, pimpath_size;

	pimpath_size = strlen(installer->pimpath);
	length = 0;

	while (*p) {
		if (!inside_percents && *p == '%') {
			p_begin = p;
			inside_percents = true;
		}
		else if (inside_percents && *p == '%') {
			p_end = p;
			length = length - (p_end - p_begin) + pimpath_size;
			inside_percents = false;
		}
		p++;
		length++;
	}

	p = ch;
	char *result = arena_new(arena, char, length + 1);
	char *q = result;
	while (*p) {
		if (!inside_percents) {
			if (*p != '%') {
				*q++ = *p++;
			}
			else {
				p_begin = p++;
				inside_percents = true;
			}
		}
		else if (inside_percents) {
			if (*p == '%') {
				p_end = p++;
				inside_percents = false;
				if (!xstrnicmp("%pimpath%", p_begin, 9)) {
					memcpy(q, installer->pimpath, pimpath_size);
					q += pimpath_size;
				}
			}
			else {
				p++;
			}
		}
	}

	return result;
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
			struct arena scratch2 = scratch;
			format_cmd(installer, cmd);

			union command cmd_edit = *cmd;
			if (cmd_edit.type == CMD_EXEC) {
				cmd_edit.arg1 = expand_pimpath(installer, cmd->arg1, &scratch2);
			}
			result = execute_command(&cmd_edit, scratch2);

			if (NT_ERROR(result)) {
				SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)error);
				SendMessageW(installer->progress_bar, PBM_SETSTATE, PBST_ERROR, 0);
				PostMessageW(installer->install_page, IPM_ERROR, 0, result);
				return result;
			}

			if (result > 0 && result <= 255) {
				result = STATUS_UNSUCCESSFUL;
				SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)error);
				SendMessageW(installer->progress_bar, PBM_SETSTATE, PBST_ERROR, 0);
				if (installer->taskbar)
					installer->taskbar->lpVtbl->SetProgressState(
							installer->taskbar,
							installer->dialog,
							TBPF_ERROR);
				PostMessageW(installer->install_page, IPM_ERROR, 0, result);
				return result;
			}

			pos = SendMessageW(installer->progress_bar, PBM_GETPOS, 0, 0);
			SendMessageW(installer->progress_bar, PBM_SETPOS, pos+1, 0);
			if (installer->taskbar)
				installer->taskbar->lpVtbl->SetProgressValue(
						installer->taskbar,
						installer->dialog, 
						pos + 1,
						installer->cmd_count);
		}	

		SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)complete);
		SendMessageW(installer->command_memo, LB_ADDSTRING, 0, (LPARAM)L"");
	}

	if (result == PIM_STATUS_SKIPPED) result = STATUS_SUCCESS;

	PostMessageW(installer->install_page, IPM_DONE, 0, 0);

	installer->scratch = old_scratch;
	return result;
}
