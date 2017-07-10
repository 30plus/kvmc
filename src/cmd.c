#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kvmc.h>

void kvmc_help_dummy(void) {}
void kvmc_help(void);

struct kvmc_cmd_s kvmc_cmds[] = {
{ "help",	kvmc_cmd_help,		kvmc_help,		"Show usage of command"},
{ "balloon",	kvmc_cmd_balloon,	kvmc_help_balloon,	"Inflate or deflate the virtio balloon"},
{ "debug",	kvmc_cmd_debug,		kvmc_help_debug,	"Print debug information from a running instance"},
{ "ls",		kvmc_cmd_ls,		kvmc_help_ls,		"Lists guest and rootfs instances on the host"},
{ "pause",	kvmc_cmd_pause,		kvmc_help_pause,	"Pause the virtual machine"},
{ "resume",	kvmc_cmd_resume,	kvmc_help_resume,	"Resume the virtual machine"},
{ "setup",	kvmc_cmd_setup,		kvmc_help_setup,	"Setup a new virtual machine"},
{ "start",	kvmc_cmd_start,		kvmc_help_start,	"Start virtual machine instance"},
{ "stat",	kvmc_cmd_stat,		kvmc_help_stat,		"Print statistics about a running instance"},
{ "stop",	kvmc_cmd_stop,		kvmc_help_stop,		"Stop a running instance"},
{ "version",	kvmc_cmd_version,	kvmc_help_dummy,	"Print the version of the kernel tree kvm tools"},
{ NULL,		NULL,			NULL,			0 },
};

void kvmc_help(void)
{
	struct kvmc_cmd_s *cmd = &kvmc_cmds[0];
	puts(" Usage:   KVMC <command> [option ...]\n\nSupported commands:\n");
	while (cmd->name != NULL) {
		printf("    %-*s   %s\n", 8, cmd->name, cmd->desc);
		cmd++;
	}
	printf("\n  'help COMMAND' for more info.\n\n");
}

int kvmc_cmd_help(int argc, const char **argv)
{
	void (*help_func)(void) = kvmc_help;
	struct kvmc_cmd_s *p;

	if (argc) {
		p = kvmc_get_cmd(argv[0]);
		if (p->help)
			help_func = p->help;
	}
	help_func();
	return EXIT_SUCCESS;
}

/* Lookup command name in kvmc command list */
struct kvmc_cmd_s *kvmc_get_cmd(const char *cmd)
{
	struct kvmc_cmd_s *p = &kvmc_cmds[0];

	while (p->name) {
		if (!strcmp(p->name, cmd))
			return p;
		p++;
	}
	if (p->name == NULL) {
		printf("  \033[1;31m[KVMC]\033[0m command \"%s\" not supported.\n\n", cmd);
		p = &kvmc_cmds[0];	/* The first command is hard-coded to "help" */
	}
	return p;
}


int kvmc_cmd_version(int argc, const char **argv)
{
	printf("KVMC %s\n", KVMC_VERSION);
	return EXIT_SUCCESS;
}
