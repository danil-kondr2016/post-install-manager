#pragma once

#include "arena.h"
#include "repo.h"
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>

struct installer {
	struct repository repo;
	struct arena scratch;

	PROPSHEETHEADERW psh;
	PROPSHEETPAGEW psp[3];

	HINSTANCE instance;
	HIMAGELIST imglist;
	HWND dialog;
	HWND software_list;
	HWND progress_bar;
	HWND command_memo;
	HWND installed_software;
	HWND install_page;
	HANDLE thread;
	
	intptr_t cmd_count;
	char *pimpath;

	ITaskbarList3 *taskbar;
	uint16_t taskbar_button_created;
};

uint32_t run_installer(struct installer *installer, struct arena *perm,
		struct arena scratch);
