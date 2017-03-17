#include <stdbool.h>
#include <linux/types.h>
#include <linux/err.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <getopt.h>

#include <kvmm.h>
#include "kvm/virtio-balloon.h"
#include "kvm/virtio-console.h"
#include "kvm/8250-serial.h"
#include "kvm/framebuffer.h"
#include "kvm/disk-image.h"
#include "kvm/threadpool.h"
#include "kvm/virtio-scsi.h"
#include "kvm/virtio-blk.h"
#include "kvm/virtio-net.h"
#include "kvm/virtio-rng.h"
#include "kvm/ioeventfd.h"
#include "kvm/virtio-9p.h"
#include "kvm/barrier.h"
#include "kvm/kvm-cpu.h"
#include "kvm/ioport.h"
#include "kvm/symbol.h"
#include "kvm/i8042.h"
#include "kvm/mutex.h"
#include "kvm/term.h"
#include "kvm/util.h"
#include "kvm/strbuf.h"
#include "kvm/vesa.h"
#include "kvm/irq.h"
#include "kvm/kvm.h"
#include "kvm/pci.h"
#include "kvm/rtc.h"
#include "kvm/sdl.h"
#include "kvm/vnc.h"
#include "kvm/guest_compat.h"
#include "kvm/pci-shmem.h"
#include "kvm/kvm-ipc.h"

#define MB_SHIFT		(20)
#define RAM_SIZE_RATIO		0.8

__thread struct kvm_cpu *current_kvm_cpu;
static int  kvm_run_wrapper;
bool do_debug_print = false;
static const char * const run_usage[] = {"KVMM start [<options>] [<kernel image>]", NULL};
static char kernel[PATH_MAX];
static const char *host_kernels[] = {"/boot/vmlinuz", "/vmlinuz", NULL};
static const char *default_kernels[] = {"../example/bzImage", "../../arch/" BUILD_ARCH "/boot/bzImage", NULL};
static const char *default_vmlinux[] = {"vmlinux", "../../../vmlinux", "../../vmlinux", NULL};

static int img_name_parser(const char *arg, struct kvm *kvm)
{
	char path[PATH_MAX];
	struct stat st;

	snprintf(path, PATH_MAX, "%s%s", kvm__get_dir(), arg);

	if ((stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) || (stat(path, &st) == 0 && S_ISDIR(st.st_mode)))
		return virtio_9p_img_name_parser(arg, kvm);
	return disk_img_name_parser(arg, kvm);
}

static void *kvm_cpu_thread(void *arg)
{
	char name[16];

	current_kvm_cpu = arg;

	sprintf(name, "kvm-vcpu-%lu", current_kvm_cpu->cpu_id);
	kvm__set_thread_name(name);

	if (kvm_cpu__start(current_kvm_cpu))
		goto panic_kvm;

	return (void *) (intptr_t) 0;

panic_kvm:
	fprintf(stderr, "KVM exit reason: %u (\"%s\")\n",
		current_kvm_cpu->kvm_run->exit_reason,
		kvm_exit_reasons[current_kvm_cpu->kvm_run->exit_reason]);
	if (current_kvm_cpu->kvm_run->exit_reason == KVM_EXIT_UNKNOWN)
		fprintf(stderr, "KVM exit code: 0x%llu\n",
			(unsigned long long)current_kvm_cpu->kvm_run->hw.hardware_exit_reason);

	kvm_cpu__set_debug_fd(STDOUT_FILENO);
	kvm_cpu__show_registers(current_kvm_cpu);
	kvm_cpu__show_code(current_kvm_cpu);
	kvm_cpu__show_page_tables(current_kvm_cpu);

	return (void *) (intptr_t) 1;
}

static void kernel_usage_with_options(void)
{
	const char **k;
	struct utsname uts;

	fprintf(stderr, "Fatal: could not find default kernel image in:\n");
	k = &default_kernels[0];
	while (*k) {
		fprintf(stderr, "\t%s\n", *k);
		k++;
	}

	if (uname(&uts) < 0)
		return;

	k = &host_kernels[0];
	while (*k) {
		if (snprintf(kernel, PATH_MAX, "%s-%s", *k, uts.release) < 0)
			return;
		fprintf(stderr, "\t%s\n", kernel);
		k++;
	}
	fprintf(stderr, "\nPlease see '%s run --help' for more options.\n\n",
		KVM_BINARY_NAME);
}

static u64 host_ram_size(void)
{
	long page_size;
	long nr_pages;

	nr_pages	= sysconf(_SC_PHYS_PAGES);
	if (nr_pages < 0) {
		pr_warning("sysconf(_SC_PHYS_PAGES) failed");
		return 0;
	}

	page_size	= sysconf(_SC_PAGE_SIZE);
	if (page_size < 0) {
		pr_warning("sysconf(_SC_PAGE_SIZE) failed");
		return 0;
	}

	return (nr_pages * page_size) >> MB_SHIFT;
}

/* If user didn't specify how much memory it wants to allocate for the guest, avoid filling the whole host RAM. */
static u64 get_ram_size(int nr_cpus)
{
	u64 ram_size = 64 * (nr_cpus + 3);
	u64 available = host_ram_size() * RAM_SIZE_RATIO;
	if (!available)
		available = MIN_RAM_SIZE_MB;
	if (ram_size > available)
		ram_size	= available;

	return ram_size;
}

static const char *find_kernel(void)
{
	const char **k;
	struct stat st;
	struct utsname uts;

	k = &default_kernels[0];
	while (*k) {
		if (stat(*k, &st) < 0 || !S_ISREG(st.st_mode)) {
			k++;
			continue;
		}
		strncpy(kernel, *k, PATH_MAX);
		return kernel;
	}

	if (uname(&uts) < 0)
		return NULL;

	k = &host_kernels[0];
	while (*k) {
		if (snprintf(kernel, PATH_MAX, "%s-%s", *k, uts.release) < 0)
			return NULL;

		if (stat(kernel, &st) < 0 || !S_ISREG(st.st_mode)) {
			k++;
			continue;
		}
		return kernel;

	}
	return NULL;
}

static const char *find_vmlinux(void)
{
	const char **vmlinux;

	vmlinux = &default_vmlinux[0];
	while (*vmlinux) {
		struct stat st;

		if (stat(*vmlinux, &st) < 0 || !S_ISREG(st.st_mode)) {
			vmlinux++;
			continue;
		}
		return *vmlinux;
	}
	return NULL;
}

void kvmm_help_start(void)
{
	puts(" Usage:   KVMM start <option [...]>\n\n"
"Basic options:\n"
"   -N, --name	<guest name>	A name for the guest\n"
"   -C, --cpus <n>		Number of CPUs\n"
"   -m, --mem <n>		Virtual machine memory size in MiB.\n"
"   -S,  -shmem <[pci:]<addr>:<size>[:handle=<handle>][:create]>\n"
"				Share host shmem with guest via pci device\n"
"   -D, --disk <path>		Disk  image or rootfs directory\n"
"   -b, --balloon		Enable virtio balloon\n"
"   -v, --vnc			Enable VNC framebuffer\n"
"   -g, --gtk			Enable GTK framebuffer\n"
"   -s, --sdl			Enable SDL framebuffer\n"
"   -r, --rng			Enable virtio Random Number Generator\n"
"   -9, --9p <share_dir,tag>	Enable virtio 9p to share files between host and guest\n"
"   -c, --console <type>	Console to use(serial, virtio or hv)\n"
"   -d, --dev <device_file>	KVM device file\n"
"   -t, --tty <tty id>		Remap guest TTY into a pty on the host\n"
"   -h, --hugetlbfs <path>	Hugetlbfs path\n"
"\nKernel options:\n"
"   -k, --kernel <kernel>	Kernel to boot in virtual machine\n"
"   -i, --initrd <initrd>	Initial RAM disk image\n"
"   -p, --params <params>	Kernel command line arguments\n"
"   -f, --firmware <firmware>	Firmware image to boot in virtual machine\n"
"\nNetworking options:\n"
"   -n, --network <params>	Create a new guest NIC\n"
"   -p, --no-dhcp		Disable kernel DHCP in rootfs mode\n"
"\nDebug options:\n"
"   -T, --debug			Enable debug messages\n"
"   -P, --debug-single-step	Enable single stepping\n"
"   -I, --debug-ioport		Enable ioport debugging\n"
"   -M, --debug-mmio		Enable MMIO debugging\n"
"   -y, --debug-iodelay <n>	Delay IO by millisecond\n\n");
//"\nArch-specific options:\n"
//"BIOS options:\n"
//"       --vidmode <n>     	Video mode\n\n");
}

static void resolve_program(const char *src, char *dst, size_t len)
{
	struct stat st;
	int err;

	err = stat(src, &st);

	if (!err && S_ISREG(st.st_mode)) {
		char resolved_path[PATH_MAX];

		if (!realpath(src, resolved_path))
			die("Unable to resolve program %s: %s\n", src, strerror(errno));

		snprintf(dst, len, "/host%s", resolved_path);
	} else
		strncpy(dst, src, len);
}

static struct kvm *kvmm_start_init(int argc, char * const* argv)
{
	int opt_idx = 0, opt;
	static char real_cmdline[2048], default_name[20];
	unsigned int nr_online_cpus;
	const struct option start_options[] = {
		{"9p",		required_argument,	0, '9'},
		{"balloon",	no_argument,		0, 'b'},
		{"console",	required_argument,	0, 'c'},
		{"cpus",	required_argument,	0, 'C'},
		{"dev",		required_argument,	0, 'd'},
		{"disk",	required_argument,	0, 'D'},
		{"gtk",		no_argument,		0, 'g'},
		{"hugetlbfs",	required_argument,	0, 'h'},
		{"mem",		required_argument,	0, 'm'},
		{"name",	required_argument,	0, 'N'},
		{"rng",		no_argument,		0, 'r'},
		{"script",	required_argument,	0, 'R'},
		{"sdl",		no_argument,		0, 's'},
		{"shmem",	required_argument,	0, 'S'},
		{"tty",		required_argument,	0, 't'},
		{"vnc",		no_argument,		0, 'v'},
/* Kernel options */
		{"cmdline",	required_argument,	0, 'e'},
		{"firmware",	required_argument,	0, 'f'},
		{"initrd",	required_argument,	0, 'i'},
		{"kernel",	required_argument,	0, 'k'},
/* Networking options */
		{"network",	required_argument,	0, 'n'},
		{"nodhcp",	no_argument,		0, 'p'},
/* Debug options */
		{"debug-ioport",no_argument,		0, 'I'},
		{"debug-mmio",	no_argument,		0, 'M'},
		{"debug",	no_argument,		0, 'T'},
		{"single-step",	no_argument,		0, 'P'},
		{"iodelay",	required_argument,	0, 'y'},
		{0,		0,			0, 0 }
	};
	const char *start_opts = "9:bc:C:d:D:e:f:gh:i:Ik:m:Mn:N:pPrR:sS:t:Tvy";

	struct kvm *kvm = kvm__new();
	if (IS_ERR(kvm))
		return kvm;

	nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	kvm->cfg.custom_rootfs_name = "default";

	optind = -1;	// TODO: getopt_long swallows the 1st argument
	while (1) {
		opt = getopt_long(argc, argv, start_opts, start_options, &opt_idx);
		if (opt == -1)
			break;
		switch (opt) {
		case '9':	virtio_9p_rootdir_parser(optarg, kvm); break;//TODO
		case 'b':	kvm->cfg.balloon = true; break;
		case 'c':	kvm->cfg.console = optarg; break;
		case 'C':	kvm->cfg.nrcpus = atoi(optarg); break;
		case 'd':	kvm->cfg.dev = optarg; break;
		case 'D':	img_name_parser(optarg, kvm); break;//TODO
		case 'e':	kvm->cfg.kernel_cmdline = optarg; break;
		case 'f':	kvm->cfg.firmware_filename = optarg; break;
		case 'g':	kvm->cfg.gtk = true; break;
		case 'h':	kvm->cfg.hugetlbfs_path = optarg; break;
		case 'm':	kvm->cfg.ram_size = atoi(optarg); break;
		case 'N':	kvm->cfg.guest_name = optarg; break;
		case 'r':	kvm->cfg.virtio_rng = true; break;
		case 's':	kvm->cfg.sdl = true; break;
		case 'S':	shmem_parser(optarg); break;//TODO
		case 't':	tty_parser(optarg); break;//TODO
		case 'v':	kvm->cfg.vnc = true; break;
		case 'i':	kvm->cfg.initrd_filename = optarg; break;
		case 'k':	kvm->cfg.kernel_filename = optarg; break;
		case 'n':	netdev_parser(optarg, kvm); break;//TODO
		case 'p':	kvm->cfg.no_dhcp = true; break;
		case 'I':	kvm->cfg.ioport_debug = true; break;
		case 'M':	kvm->cfg.mmio_debug = true; break;
		case 'T':	do_debug_print = true; break;
		case 'P':	kvm->cfg.single_step = true; break;
		case 'y':	kvm->cfg.debug_iodelay = atoi(optarg); break;
		default:	break;
		}
	}
//	if ((optind < argc) && !(kvm->cfg.kernel_filename))
//		kvm->cfg.kernel_filename = argv[optind];

	kvm->nr_disks = kvm->cfg.image_count;

	if (!kvm->cfg.kernel_filename)
		kvm->cfg.kernel_filename = find_kernel();
	if (!kvm->cfg.kernel_filename) {
		kernel_usage_with_options();
		return ERR_PTR(-EINVAL);
	}

	kvm->cfg.vmlinux_filename = find_vmlinux();
	kvm->vmlinux = kvm->cfg.vmlinux_filename;

	if (kvm->cfg.nrcpus == 0)
		kvm->cfg.nrcpus = nr_online_cpus;

	if (!kvm->cfg.ram_size)
		kvm->cfg.ram_size = get_ram_size(kvm->cfg.nrcpus);

	if (kvm->cfg.ram_size > host_ram_size())
		pr_warning("Guest memory size %lluMB exceeds host physical RAM size %lluMB",
			(unsigned long long)kvm->cfg.ram_size, (unsigned long long)host_ram_size());

	kvm->cfg.ram_size <<= MB_SHIFT;

	if (!kvm->cfg.dev)
		kvm->cfg.dev = DEFAULT_KVM_DEV;

	if (!kvm->cfg.console)
		kvm->cfg.console = DEFAULT_CONSOLE;

	if (!strncmp(kvm->cfg.console, "virtio", 6))
		kvm->cfg.active_console  = CONSOLE_VIRTIO;
	else if (!strncmp(kvm->cfg.console, "serial", 6))
		kvm->cfg.active_console  = CONSOLE_8250;
	else if (!strncmp(kvm->cfg.console, "hv", 2))
		kvm->cfg.active_console = CONSOLE_HV;
	else
		pr_warning("No console!");

	if (!kvm->cfg.host_ip)
		kvm->cfg.host_ip = DEFAULT_HOST_ADDR;

	if (!kvm->cfg.guest_ip)
		kvm->cfg.guest_ip = DEFAULT_GUEST_ADDR;

	if (!kvm->cfg.guest_mac)
		kvm->cfg.guest_mac = DEFAULT_GUEST_MAC;

	if (!kvm->cfg.host_mac)
		kvm->cfg.host_mac = DEFAULT_HOST_MAC;

	if (!kvm->cfg.script)
		kvm->cfg.script = DEFAULT_SCRIPT;

	if (!kvm->cfg.network)
                kvm->cfg.network = DEFAULT_NETWORK;

	memset(real_cmdline, 0, sizeof(real_cmdline));
	kvm__arch_set_cmdline(real_cmdline, kvm->cfg.vnc || kvm->cfg.sdl || kvm->cfg.gtk);

	if (!kvm->cfg.guest_name) {
		if (kvm->cfg.custom_rootfs) {
			kvm->cfg.guest_name = kvm->cfg.custom_rootfs_name;
		} else {
			sprintf(default_name, "guest-%u", getpid());
			kvm->cfg.guest_name = default_name;
		}
	}

	if (!kvm->cfg.using_rootfs && !kvm->cfg.disk_image[0].filename && !kvm->cfg.initrd_filename) {
		char tmp[PATH_MAX];

		kvm_setup_create_new(kvm->cfg.custom_rootfs_name);
		kvm_setup_resolv(kvm->cfg.custom_rootfs_name);

		snprintf(tmp, PATH_MAX, "%s%s", kvm__get_dir(), "default");
		if (virtio_9p__register(kvm, tmp, "/dev/root") < 0)
			die("Unable to initialize virtio 9p");
		if (virtio_9p__register(kvm, "/", "hostfs") < 0)
			die("Unable to initialize virtio 9p");
		kvm->cfg.using_rootfs = kvm->cfg.custom_rootfs = 1;
	}

	if (kvm->cfg.using_rootfs) {
		strcat(real_cmdline, " rw rootflags=trans=virtio,version=9p2000.L,cache=loose rootfstype=9p");
		if (kvm->cfg.custom_rootfs) {
#ifdef CONFIG_GUEST_PRE_INIT
			strcat(real_cmdline, " init=/virt/pre_init");
#else
			strcat(real_cmdline, " init=/virt/init");
#endif

			if (!kvm->cfg.no_dhcp)
				strcat(real_cmdline, "  ip=dhcp");
			if (kvm_setup_guest_init(kvm->cfg.custom_rootfs_name))
				die("Failed to setup init for guest.");
		}
	} else if (!kvm->cfg.kernel_cmdline || !strstr(kvm->cfg.kernel_cmdline, "root=")) {
		strlcat(real_cmdline, " root=/dev/vda rw ", sizeof(real_cmdline));
	}

	if (kvm->cfg.kernel_cmdline) {
		strcat(real_cmdline, " ");
		strlcat(real_cmdline, kvm->cfg.kernel_cmdline, sizeof(real_cmdline));
	}

	kvm->cfg.real_cmdline = real_cmdline;

	printf("  # %s start -k %s -m %Lu -c %d --name %s\n", KVM_BINARY_NAME,
		kvm->cfg.kernel_filename,
		(unsigned long long)kvm->cfg.ram_size / 1024 / 1024,
		kvm->cfg.nrcpus, kvm->cfg.guest_name);

	if (init_list__init(kvm) < 0)
		die ("Initialisation failed");

	return kvm;
}

int kvmm_cmd_start(int argc, const char ** argv)
{
	int ret = -EFAULT;
	struct kvm *kvm = kvmm_start_init(argc, (char * const*)argv);
	if (IS_ERR(kvm))
		return PTR_ERR(kvm);

	for (int i = 0; i < kvm->nrcpus; i++) {
		if (pthread_create(&kvm->cpus[i]->thread, NULL, kvm_cpu_thread, kvm->cpus[i]) != 0)
			die("unable to create KVM VCPU thread");
	}

	/* Wait for guest to exit */
	if (pthread_join(kvm->cpus[0]->thread, NULL) != 0)
		die("unable to join with vcpu 0");
	ret = kvm_cpu__exit(kvm);

	compat__print_all_messages();
	init_list__exit(kvm);
	if (ret == 0)
		printf("\n  # KVM session ended normally.\n");

	return ret;
}
