#include <kvmc.h>
#include <kvm/kvm-ipc.h>
#include <stdio.h>

void kvmc_help_pause(void)
{
	puts(" Usage:   KVMC pause [name ...]\n\n\tPauses all instances if name is not specified.\n");
}

static int do_pause(const char *name, int sock)
{
	int r = 0;

	int vmstate = get_vmstate(sock);
	if (vmstate < 0)
		return vmstate;
	if (vmstate == KVM_VMSTATE_PAUSED) {
		printf("Guest %s is already paused.\n", name);
		return 0;
	}

	r = kvm_ipc__send(sock, KVM_IPC_PAUSE);
	if (r)
		return r;

	printf("Guest %s paused\n", name);
	return 0;
}

int kvmc_cmd_pause(int argc, const char **argv)
{
	int vm = 0, r = 0;

	if (argc == 0)
		return kvmc_for_each(do_pause);

	while (argc--) {
		vm = kvmc_get_by_name(argv[argc]);
		if (vm <= 0) {
			printf("  \033[1;33m[WARN]\033[0m Couldn't find running instance '%s'.\n", argv[argc]);
			continue;
		}
		r += do_pause(argv[argc], vm);
		close(vm);
	}
	return r;
}
