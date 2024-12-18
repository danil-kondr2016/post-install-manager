#include <stdio.h>
#include "repo.h"
#include "install.h"
#include <commctrl.h>

int main(void)
{
	struct repository repo = {0};
	struct arena perm, scratch;
	InitCommonControls();

	perm = arena_create(8192<<10);
	scratch = arena_create(8192<<10);

/*	repository_parse(&repo, "test.xml", &perm, scratch);
	int i = 1;

	for (struct program *prog = repo.head; prog; prog = prog->next, i++) {
		printf("Program #%d\n", i);
		printf("  Name: %s\n", prog->name);
		for (union command *cmd = prog->cmd; cmd; cmd = cmd->next) {
			printf("  Command (%08x,%08x): ", cmd->arch, cmd->os);
			switch (cmd->type) {
			case CMD_EXEC: printf("{exec} %s %s\n", cmd->exec.path, cmd->exec.args); break;
			case CMD_CMD:  printf("{cmd} %s\n", cmd->cmd.line); break;
			case CMD_MKDIR: printf("{mkdir} %s\n", cmd->mkdir.path); break;
			case CMD_RMDIR: printf("{rmdir} %s\n", cmd->rmdir.path); break;
			case CMD_RMFILE: printf("{rmfile} %s\n", cmd->rmfile.path); break;
			case CMD_CPFILE: printf("{cpfile} %s %s\n", cmd->cpfile.from, cmd->cpfile.to); break;
			case CMD_CPDIR: printf("{cpdir} %s %s\n", cmd->cpdir.from, cmd->cpdir.to); break;
			case CMD_MOVE: printf("{move} %s %s\n", cmd->move.from, cmd->move.to); break;
			case CMD_RENAME: printf("{rename} %s %s\n", cmd->rename.path, cmd->rename.newname); break;
			case CMD_ALERT: printf("{alert} %s\n", cmd->alert.msg); break;
			case CMD_FAIL: printf("{fail} %s\n", cmd->fail.msg); break;
			default:
				       printf("type #%d\n", cmd->type);
			}
		}
	}
*/
	struct installer installer = {0};
	installer.scratch = scratch;
	installer.instance = GetModuleHandleW(NULL);
	run_installer(&installer, &perm, scratch);

	return 0;
}
