#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <kvmm.h>
#include "kvm/util.h"

#define KVMM_VERSION    "v0.1.d"

void kvmm_help_dummy(void) {}
void kvmm_help(void);

struct kvmm_cmd_s kvmm_cmds[] = {
	{ "help",	kvmm_cmd_help,		kvmm_help,		"Show usage of command"},
	{ "balloon",	kvmm_cmd_balloon,	kvmm_help_balloon,	"Inflate or deflate the virtio balloon"},
	{ "debug",	kvmm_cmd_debug,		kvmm_help_debug,	"Print debug information from a running instance"},
	{ "ls",		kvmm_cmd_ls,		kvmm_help_ls,		"Lists guest and rootfs instances on the host"},
	{ "pause",	kvmm_cmd_pause,		kvmm_help_pause,	"Pause the virtual machine"},
	{ "resume",	kvmm_cmd_resume,	kvmm_help_resume,	"Resume the virtual machine"},
	{ "setup",	kvmm_cmd_setup,		kvmm_help_setup,	"Setup a new virtual machine"},
	{ "start",	kvmm_cmd_start,		kvmm_help_start,	"Start virtual machine instance"},
	{ "stat",	kvmm_cmd_stat,		kvmm_help_stat,		"Print statistics about a running instance"},
	{ "stop",	kvmm_cmd_stop,		kvmm_help_stop,		"Stop a running instance"},
	{ "version",	kvmm_cmd_version,	kvmm_help_dummy,	"Print the version of the kernel tree kvm tools"},
	{ NULL,		NULL,			NULL,			0 },
};

void kvmm_help(void)
{
	struct kvmm_cmd_s *cmd = &kvmm_cmds[0];
	puts("Supported commands:\n");
	while (cmd->name != NULL) {
		printf("    %-*s   %s\n", 8, cmd->name, cmd->desc);
		cmd++;
	}
	printf("\n  'help COMMAND' for more info.\n\n");
}

int kvmm_cmd_help(int argc, const char **argv)
{
	void (*help_func)(void) = kvmm_help;
	struct kvmm_cmd_s *p;

	if (argc) {
		p = kvmm_get_cmd(argv[0]);
		if (p->help)
			help_func = p->help;
	}
	help_func();
	return EXIT_SUCCESS;
}

/* Translate command name to record structure */
struct kvmm_cmd_s *kvmm_get_cmd(const char *cmd)
{
	struct kvmm_cmd_s *p = &kvmm_cmds[0];

	while (p->name) {
		if (!strcmp(p->name, cmd))
			return p;
		p++;
	}
	if (p->name == NULL) {
		BUG_ON(!p);
		printf("  \033[1;31m[KVMM]\033[0m command \"%s\" not supported.\n\n", cmd);
		p = &kvmm_cmds[0];
	}
	return p;
}


int kvmm_cmd_version(int argc, const char **argv)
{
	printf("KVMM %s\n", KVMM_VERSION);
	return EXIT_SUCCESS;
}
