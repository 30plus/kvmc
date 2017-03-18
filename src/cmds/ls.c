#include <kvmc.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

void kvmc_help_ls(void)
{
	puts(" Usage:   KVMC ls [-r] [-f]\n\n"
		"\t'-r' for guest and '-f' for rootfs, default action is to list both.\n");
}

int get_vmstate(int sock)
{
	int vmstate;
	int r;

	r = kvm_ipc__send(sock, KVM_IPC_VMSTATE);
	if (r < 0)
		return r;

	r = read(sock, &vmstate, sizeof(vmstate));
	if (r < 0)
		return r;

	return vmstate;
}

static int print_guest(const char *name, int sock)
{
	int r = 0;
	pid_t pid = 0;

	r = kvm_ipc__send(sock, KVM_IPC_PID);
	if (r < 0)
		return r;
	r = read(sock, &pid, sizeof(pid));
	if (r < 0)
		return r;

	int vmstate = get_vmstate(sock);

	if ((int)pid < 0 || vmstate < 0) {
		perror("Error listing instances");
		return -1;
	}

	printf("%5d %-20s %s\n", pid, name, ((vmstate == KVM_VMSTATE_PAUSED) ? "paused" : "running"));
	return 0;
}

static void kvm_ls_rootfs(void)
{
	struct dirent *dirent;
	DIR *dir = opendir(kvm__get_dir());
	if (dir == NULL) {
		perror("Error listing rootfs");
		return;
	}

	while ((dirent = readdir(dir))) {
		if (dirent->d_type == DT_DIR && strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, ".."))
			printf("%6s %-20s %s\n", "", dirent->d_name, "shutoff");
	}
}

int kvmc_cmd_ls(int argc, const char **argv)
{
	bool guest = false, rootfs = false;

	while (argc--) {
		if (!strcmp(argv[argc], "-r"))
			guest = true;
		if (!strcmp(argv[argc], "-f"))
			rootfs = true;
	}
	if ((guest == false) && (rootfs == false))
		guest = rootfs = true;

	printf("%6s %-20s %s\n", "PID", "NAME", "STATE");
	if (guest) {
		puts("------------ Guests ----------------");
		kvmc_for_each(print_guest);
	}
	if (rootfs) {
		puts("------------ rootfs ----------------");
		kvm_ls_rootfs();
	}
	return 0;
}
