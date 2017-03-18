#include <kvmc.h>
#include <kvm/kvm-ipc.h>
#include <stdio.h>

void kvmc_help_resume(void)
{
	puts(" Usage:	KVMC resume [name ...]\n\n\tResumes all instances if name is not specified.\n");
}

static int do_resume(const char *name, int sock)
{
	int r;
	int vmstate;

	vmstate = get_vmstate(sock);
	if (vmstate < 0)
		return vmstate;
	if (vmstate == KVM_VMSTATE_RUNNING) {
		printf("Guest %s is still running.\n", name);
		return 0;
	}

	r = kvm_ipc__send(sock, KVM_IPC_RESUME);
	if (r)
		return r;

	printf("Guest %s resumed\n", name);

	return 0;
}

int kvmc_cmd_resume(int argc, const char **argv)
{
	int vm, r = 0;

	if (argc == 0)
		return kvmc_for_each(do_resume);

	while (argc--) {
		vm = kvmc_get_by_name(argv[argc]);
		if (vm <= 0) {
			printf("  \033[1;33m[WARN]\033[0m Paused instance '%s' not found.\n", argv[argc]);
			continue;
		}
		r += do_resume(argv[argc], vm);
		close(vm);
	}
	return r;
}
