#include <stdio.h>

#include "arena.h"
#include "fatal.h"
#include "install.h"

#include <windows.h>
#include <commctrl.h>

int wWinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPWSTR szCmdLine, int nCmdShow)
{
	struct arena perm, scratch;
	struct installer installer = {0};
	uint32_t result = 0;

	InitCommonControls();
	LoadLibraryA("ntdll.dll");

	perm = arena_create(8192<<10);
	scratch = arena_create(8192<<10);

	installer.scratch = scratch;
	installer.instance = hThisInstance;
	result = run_installer(&installer, &perm, scratch);
	if ((result & 0xC0000000) == 0xC0000000) {
		error_msgW(NULL, result);
		return 1;
	}

	return 0;
}
