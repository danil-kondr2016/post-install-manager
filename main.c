#include "stdafx.h"

#include "runner.h"
#include "command.h"

void CallAndShowStatus(PCMDRUNNER Runner, char *command)
{
    HRESULT Result;
    DWORD Status;
    LPWSTR CommandWide;
    DWORD CommandWideSize;
    WCHAR StatusString[256];
    LPWSTR MessageString;
    DWORD MessageStringSize;

    CommandWideSize = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
    CommandWide = (LPWSTR)calloc(CommandWideSize + 1, sizeof(WCHAR));
    if (!CommandWide)
        return;
    MultiByteToWideChar(CP_UTF8, 0, command, -1, CommandWide, CommandWideSize);

    Result = ExecuteCommand(Runner, command, &Status);
    swprintf_s(StatusString, 256, L"Result 0x%08X, Status 0x%08X", Result, Status);
    MessageStringSize = wcslen(StatusString) + wcslen(CommandWide) + 3;
    MessageString = (LPWSTR)calloc(MessageStringSize + 1, sizeof(WCHAR));
    if (!MessageString) {
        free(CommandWide);
        return;
    }
    swprintf_s(MessageString, MessageStringSize + 1, L"%s\n%s", CommandWide, StatusString);
    free(CommandWide);

    MessageBoxW(NULL, MessageString, L"Status", MB_OK);
    free(MessageString);
}


int main(int argc, char **argv)
{
	CMDRUNNER Runner = { 0 };
	DWORD Status;

	InitCommandRunner(&Runner);
    CallAndShowStatus(&Runner, "Run cmd.exe /c echo Hello && pause");
    CallAndShowStatus(&Runner, "Run cmd.exe /c echo Test Test && pause");
    CallAndShowStatus(&Runner, "Run cmd.exe /c echo Test Test Test && pause");
    CallAndShowStatus(&Runner, "MakeDir d");
    CallAndShowStatus(&Runner, "Run cmd.exe /c echo a > a");
    CallAndShowStatus(&Runner, "Run cmd.exe /c echo b > b");
    CallAndShowStatus(&Runner, "Run cmd.exe /c echo c > c");
    CallAndShowStatus(&Runner, "CopyFile a d");
    CallAndShowStatus(&Runner, "CopyFile b d");
    CallAndShowStatus(&Runner, "CopyFile c d");

	MessageBox(NULL, "Hello, world!", "Проверка UTF-8", MB_OK);
	return 0;
}