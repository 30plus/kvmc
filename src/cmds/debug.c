#include <kvmm.h>
#include <kvm/util.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>
#include <kvm/read-write.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define BUFFER_SIZE 100

void kvmm_help_debug(void)
{
	puts(" Usage:   KVMM debug <name> [-d] [-m] [-s]\n\n"
		"\t'-d' generates a debug dump from guest\n"
		"\t'-m' generates NMI on vCPU\n"
		"\t'-s' injects a sysrq\n");
}

int kvmm_cmd_debug(int argc, const char **argv)
{
	int instance, r, opt;
	char buff[BUFFER_SIZE];
	struct debug_cmd_params cmd = {.dbg_type = 0};

	if (argc < 2) {
		printf("  \033[1;33m[WARN]\033[0m Should specify guest name and at least one action.\n");
		return -1;
	}

	instance = kvm__get_sock_by_instance(argv[0]);
	if (instance <= 0) {
		printf("  \033[1;33m[WARN]\033[0m Running instance '%s' not found.\n", argv[0]);
		return -1;
	}

	while ((opt = getopt(argc-1, (char * const*)&argv[1], "dm:s:")) != -1) {
		switch (opt) {
		case 'd':
			cmd.dbg_type |= KVM_DEBUG_CMD_TYPE_DUMP;
			break;
		case 'm':
			cmd.dbg_type |= KVM_DEBUG_CMD_TYPE_NMI;
			cmd.cpu = atoi(optarg);
			break;
		case 's':
			cmd.dbg_type |= KVM_DEBUG_CMD_TYPE_SYSRQ;
			cmd.sysrq = optarg[0];
			break;
		default:
			continue;
		}
	}

	r = kvm_ipc__send_msg(instance, KVM_IPC_DEBUG, sizeof(cmd), (u8 *)&cmd);
	if (r < 0)
		return r;
	if (!(cmd.dbg_type & KVM_DEBUG_CMD_TYPE_DUMP))	/* Don't dump */
		return 0;

	do {
		r = xread(instance, buff, BUFFER_SIZE);
		if (r < 0)
			return 0;
		printf("%.*s", r, buff);
	} while (r > 0);
	close(instance);
	return r;
}
