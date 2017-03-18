#include <kvmc.h>
#include <kvm/kvm-ipc.h>
#include <stdio.h>

void kvmc_help_stop(void)
{
	puts(" Usage:	KVMC stop [name ...]\n\n\tStops all instances if name is not specified.\n");
}

static int do_stop(const char *name, int sock)
{
	return kvm_ipc__send(sock, KVM_IPC_STOP);
}

int kvmc_cmd_stop(int argc, const char **argv)
{
	int vm, r = 0;

	if (argc == 0)
		return kvmc_for_each(do_stop);

	while (argc--) {
		vm = kvmc_get_by_name(argv[argc]);
		if (vm <= 0) {
			printf("  \033[1;33m[WARN]\033[0m Instance '%s' not found.\n", argv[argc]);
			continue;
		}
		r += do_stop(argv[argc], vm);
		close(vm);
	}
	return r;
}
