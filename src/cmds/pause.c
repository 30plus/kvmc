#include <kvmm.h>
#include <kvm/util.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

void kvmm_help_pause(void)
{
	puts(" Usage:   KVMM pause [name ...]\n\n\tPauses all instances if name is not specified.\n");
}

static int do_pause(const char *name, int sock)
{
	int r, vmstate;

	vmstate = get_vmstate(sock);
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

int kvmm_cmd_pause(int argc, const char **argv)
{
	int instance, r;

	if (argc == 0)
		return kvm__enumerate_instances(do_pause);

	while (argc--) {
		instance = kvm__get_sock_by_instance(argv[argc]);
		if (instance <= 0) {
			printf("  \033[1;33m[WARN]\033[0m Couldn't find running instance '%s'.\n", argv[argc]);
			continue;
		}
		r += do_pause(argv[argc], instance);
		close(instance);
	}
	return r;
}
