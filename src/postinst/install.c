#define UNICODE  1
#define _UNICODE 1

#include "resource.h"
#include "install.h"
#include "commands.h"

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
		return 0xC000000D;

	result = repository_parse(&installer->repo, "pim.xml", perm, scratch);
	if ((result & 0xC0000000) == 0xC0000000)
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

	switch (msg) {
	case WM_INITDIALOG:
		page = (PROPSHEETPAGE *)lParam;
		installer = (struct installer *)page->lParam;
		SetWindowLongPtrW(hWnd, DWLP_USER, (LONG_PTR)installer);
		installer->progress_bar = GetDlgItem(hWnd, IDC_PROGRESS);
		installer->install_page = hWnd;
		SendMessageW(installer->progress_bar, PBM_SETRANGE32, 0, installer->prog_count);
		installer->command_memo = GetDlgItem(hWnd, IDC_COMMANDS);
		installer->installed_software = GetDlgItem(hWnd, IDC_INSTALLING);
		installer->thread = CreateThread(NULL, 32768, (LPTHREAD_START_ROUTINE)installer_thread,
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
		PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_FINISH);
		MessageBoxW(hWnd, L"Установка завершена.", L"", MB_ICONINFORMATION);
		break;
	case IPM_ERROR:
		PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_FINISH);
		MessageBoxW(hWnd, L"При установке произошла ошибка.", L"", MB_ICONINFORMATION);
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

	installer->prog_count = 0;
	for (struct program *prog = installer->repo.head;
			prog;
			prog = prog->next)
	{
		installer->prog_count++;
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

static DWORD WINAPI installer_thread(struct installer *installer)
{
	DWORD result = 0;
	DWORD pos;
	struct arena old_scratch = installer->scratch;
	wchar_t *prog_text, *prog_nameW, *prog_buf;
	size_t prog_text_size;

	prog_text_size = LoadStringW(installer->instance, IDS_INSTALLING, (LPCWSTR)&prog_text, 0);
	prog_text = arena_new(&installer->scratch, wchar_t, prog_text_size + 1);
	LoadStringW(installer->instance, IDS_INSTALLING, prog_text, prog_text_size + 1);
	for (struct program *prog = installer->repo.head;
			prog;
			prog = prog->next)
	{
		struct arena scratch = installer->scratch;
		wchar_t *prog_nameW, *prog_buf;
		size_t prog_buf_count;

		prog_nameW = u8_to_u16(prog->name, &scratch);
		prog_buf_count = prog_text_size + wcslen(prog_nameW) + 1;
		prog_buf = arena_new(&scratch, wchar_t, prog_buf_count);
		StringCchPrintfW(prog_buf, prog_buf_count, prog_text, prog_nameW);

		SetWindowTextW(installer->installed_software, prog_buf);

		result = execute_command_chain(prog->cmd, scratch);

		if ((result & 0xC0000000) == 0xC0000000) {
			SendMessageW(installer->install_page, IPM_ERROR, 0, 0);
			return result;
		}

		pos = SendMessageW(installer->progress_bar, PBM_GETPOS, 0, 0);
		SendMessageW(installer->progress_bar, PBM_SETPOS, pos+1, 0);
	}

	if (result == 0x27F10000) result = 0x00000000;

	SendMessageW(installer->install_page, IPM_DONE, 0, 0);

	installer->scratch = old_scratch;
	return result;
}
