#include <kvmm.h>
#include <kvm/util.h>
#include <kvm/kvm.h>
#include <kvm/kvm-ipc.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <linux/virtio_balloon.h>

void kvmm_help_stat(void)
{
	puts(" Usage:   KVMM stat [name]\n\n\tDisplays memory statistics, of all instances if name is not specified.\n");
}

static int do_memstat(const char *name, int sock)
{
	struct virtio_balloon_stat stats[VIRTIO_BALLOON_S_NR];
	fd_set fdset;
	struct timeval t = { .tv_sec = 1 };
	int r;
	u8 i;

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);
	r = kvm_ipc__send(sock, KVM_IPC_STAT);
	if (r < 0)
		return r;

	r = select(1, &fdset, NULL, NULL, &t);
	if (r < 0) {
		pr_err("Could not retrieve mem stats from %s", name);
		return r;
	}
	r = read(sock, &stats, sizeof(stats));
	if (r < 0)
		return r;

	printf("\n\n\t*** Guest memory statistics ***\n\n");
	for (i = 0; i < VIRTIO_BALLOON_S_NR; i++) {
		switch (stats[i].tag) {
		case VIRTIO_BALLOON_S_SWAP_IN:
			printf("The amount of memory that has been swapped in (in bytes):");
			break;
		case VIRTIO_BALLOON_S_SWAP_OUT:
			printf("The amount of memory that has been swapped out to disk (in bytes):");
			break;
		case VIRTIO_BALLOON_S_MAJFLT:
			printf("The number of major page faults that have occurred:");
			break;
		case VIRTIO_BALLOON_S_MINFLT:
			printf("The number of minor page faults that have occurred:");
			break;
		case VIRTIO_BALLOON_S_MEMFREE:
			printf("The amount of memory not being used for any purpose (in bytes):");
			break;
		case VIRTIO_BALLOON_S_MEMTOT:
			printf("The total amount of memory available (in bytes):");
			break;
		}
		printf("%llu\n", (unsigned long long)stats[i].val);
	}
	printf("\n");
	return 0;
}

int kvmm_cmd_stat(int argc, const char **argv)
{
	int instance, r = 0;

	if (argc == 0)
		return kvm__enumerate_instances(do_memstat);

	instance = kvm__get_sock_by_instance(argv[0]);
	if (instance <= 0) {
		printf("  \033[1;33m[WARN]\033[0m Running instance '%s' not found.\n", argv[0]);
		return -1;
	}

	r = do_memstat(argv[0], instance);
	close(instance);
	return r;
}
