#include <kvmc.h>
#include <kvm/util.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

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
	int instance, r = 0;

	if (argc == 0)
		return kvm__enumerate_instances(do_resume);

	while (argc--) {
		instance = kvm__get_sock_by_instance(argv[argc]);
		if (instance <= 0) {
			printf("  \033[1;33m[WARN]\033[0m Paused instance '%s' not found.\n", argv[argc]);
			continue;
		}
		r += do_resume(argv[argc], instance);
		close(instance);
	}
	return r;
}
