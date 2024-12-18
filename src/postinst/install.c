#include "resource.h"
#include "install.h"

#ifndef PSH_AEROWIZARD
#define PSH_AEROWIZARD 0x4000
#endif

#include <string.h>
#include <wchar.h>

#include <commctrl.h>
#include <uxtheme.h>
#include <vsstyle.h>

static HIMAGELIST create_imglist_checkboxes(HWND hWnd);
static void load_repository(struct installer *installer);
static INT_PTR CALLBACK select_page_proc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK PshPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
	installer->psp[1].pfnDlgProc = PshPageProc;
	installer->psp[1].pszTemplate = L"IDD_PROCESS";

	installer->psh.dwSize = sizeof(PROPSHEETHEADERW);
	installer->psh.dwFlags = PSH_WIZARD97 | PSH_PROPSHEETPAGE;
	installer->psh.nPages = 2;
	installer->psh.ppsp = installer->psp;

	PropertySheetW(&installer->psh);
	return 0;
}

static INT_PTR CALLBACK PshPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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
		installer->imglist = create_imglist_checkboxes(hWnd);
		installer->software_list = GetDlgItem(hWnd, IDC_PROGLIST);
		ListView_SetExtendedListViewStyle(installer->software_list, LVS_EX_CHECKBOXES);
		ListView_SetImageList(installer->software_list, installer->imglist, LVSIL_STATE);
		load_repository(installer);
		break;
	case WM_NOTIFY:
		lpnmhdr = (LPNMHDR)lParam;
		switch (lpnmhdr->code) {
		case PSN_SETACTIVE:
			PropSheet_SetWizButtons(GetParent(hWnd), PSWIZB_NEXT);
			break;
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

	item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
	for (struct program *prog = installer->repo.head; prog; prog = prog->next, i++) {
		struct arena scratch = installer->scratch;
		item.pszText = u8_to_u16(prog->name, &scratch);
		item.lParam = (LPARAM)prog;
		item.state = INDEXTOSTATEIMAGEMASK(1);
		item.stateMask = LVIS_STATEIMAGEMASK;

		SendMessageW(installer->software_list, LVM_INSERTITEMW, 0, (LPARAM)&item);
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
