#include "stdafx.h"

#include "runner.h"
#include "command.h"

int main(int argc, char **argv)
{
	CMDRUNNER Runner = { 0 };
	DWORD Status;

	InitCommandRunner(&Runner);
	ExecuteCommand(&Runner, "Run cmd.exe /c echo Hello && pause", &Status);
	ExecuteCommand(&Runner, "Run cmd.exe /c echo Test Test && pause", &Status);
	ExecuteCommand(&Runner, "Run cmd.exe /c echo Test Test Test && pause", &Status);

	MessageBox(NULL, "Hello, world!", "Проверка UTF-8", MB_OK);
	return 0;
}