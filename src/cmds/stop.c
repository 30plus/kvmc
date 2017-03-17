#include <kvmm.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

void kvmm_help_stop(void)
{
	puts(" Usage:	KVMM stop [name ...]\n\n\tStops all instances if name is not specified.\n");
}

static int do_stop(const char *name, int sock)
{
	return kvm_ipc__send(sock, KVM_IPC_STOP);
}

int kvmm_cmd_stop(int argc, const char **argv)
{
	int instance, r = 0;

	if (argc == 0)
		return kvm__enumerate_instances(do_stop);

	while (argc--) {
		instance = kvm__get_sock_by_instance(argv[argc]);
		if (instance <= 0) {
			printf("  \033[1;33m[WARN]\033[0m Instance '%s' not found.\n", argv[argc]);
			continue;
		}
		r += do_stop(argv[argc], instance);
		close(instance);
	}
	return r;
}
