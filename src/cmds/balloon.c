#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <kvm/util.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>

void kvmc_help_balloon(void)
{
	puts(" Usage:   KVMC balloon <name> <-i|-d> <amount>\n\n"
		"\t'-i' to inflate, '-d' to deflate, and 'amount' should be in size of MB.\n");
}

int kvmc_cmd_balloon(int argc, const char **argv)
{
	int instance, r, amount;
	u8 amount_abs = 0;

	if (argc != 3) {
		printf("  \033[1;33m[WARN]\033[0m Exactly THREE arguments required.\n\n");
		kvmc_help_balloon();
		return -1;
	}

	instance = kvm__get_sock_by_instance(argv[0]);
	if (instance <= 0) {
		printf("  \033[1;33m[WARN]\033[0m Guest instance '%s' not found.\n", argv[0]);
		return -1;
	}

	amount = atoi(argv[2]);
	if ((amount < 0) || (amount > 127)) {
		printf("  \033[1;33m[WARN]\033[0m amount should be set 0-127.\n");
		return -1;
	}

	if (!strncmp(argv[1], "-i", 3))
		amount_abs = amount;
	else if (!strncmp(argv[1], "-d", 3))
		amount_abs = -amount;
	else {
		printf("  \033[1;33m[WARN]\033[0m Actions other than inflate or deflate are not supported..\n");
		return -1;
	}

	r = kvm_ipc__send_msg(instance, KVM_IPC_BALLOON, sizeof(amount_abs), (u8 *)&amount_abs);
	close(instance);

	if (r < 0)
		return -1;
	return 0;
}
