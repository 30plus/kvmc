#include <kvmm.h>
#include <kvm/util.h>
#include <kvm/kvm.h>
#include <kvm/read-write.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>

void kvmm_help_setup(void)
{
	printf(" Usage:   KVMM setup [name]\n\n\t\tCreates new rootfs under %s.\n\n", kvm__get_dir());
}

static int copy_file(const char *from, const char *to)
{
	int in_fd, out_fd;
	void *src, *dst;
	struct stat st;
	int err = -1;

	in_fd = open(from, O_RDONLY);
	if (in_fd < 0)
		return err;

	if (fstat(in_fd, &st) < 0)
		goto error_close_in;

	out_fd = open(to, O_RDWR | O_CREAT | O_TRUNC, st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO));
	if (out_fd < 0)
		goto error_close_in;

	src = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if (src == MAP_FAILED)
		goto error_close_out;

	if (ftruncate(out_fd, st.st_size) < 0)
		goto error_munmap_src;

	dst = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, out_fd, 0);
	if (dst == MAP_FAILED)
		goto error_munmap_src;

	memcpy(dst, src, st.st_size);

	if (fsync(out_fd) < 0)
		goto error_munmap_dst;

	err = 0;

error_munmap_dst:
	munmap(dst, st.st_size);
error_munmap_src:
	munmap(src, st.st_size);
error_close_out:
	close(out_fd);
error_close_in:
	close(in_fd);

	return err;
}

static const char *guestfs_dirs[] = {"/dev", "/etc", "/home", "/host", "/proc", "/root", "/sys", "/tmp", "/var",
	"/var/lib", "/virt", "/virt/home"};
static const char *guestfs_symlinks[] = {"/bin", "/lib", "/lib64", "/sbin", "/usr", "/etc/ld.so.conf"};

#ifdef CONFIG_GUEST_INIT
int kvm_setup_guest_init(const char *guestfs_name)
{
	int err;
	char path[PATH_MAX];

#ifdef CONFIG_GUEST_PRE_INIT
	snprintf(path, PATH_MAX, "%s%s/%s", kvm__get_dir(), guestfs_name, "virt/pre_init");
	err = copy_file(KVMM_GUEST_PRE_INIT, path);
	if (err)
		return err;
#endif
	snprintf(path, PATH_MAX, "%s%s/%s", kvm__get_dir(), guestfs_name, "virt/init");
	err = copy_file(KVMM_GUEST_INIT, path);
	return err;
}
#else
int kvm_setup_guest_init(const char *guestfs_name)
{
	die("Guest init image not compiled in");
	return 0;
}
#endif

static int copy_passwd(const char *guestfs_name)
{
	char path[PATH_MAX];
	FILE *file;
	int ret;

	snprintf(path, PATH_MAX, "%s%s/etc/passwd", kvm__get_dir(), guestfs_name);

	file = fopen(path, "w");
	if (!file)
		return -1;

	ret = fprintf(file, "root:x:0:0:root:/root:/bin/sh\n");
	if (ret > 0)
		ret = 0;

	fclose(file);
	return ret;
}

static int make_guestfs_symlink(const char *guestfs_name, const char *path)
{
	char target[PATH_MAX];
	char name[PATH_MAX];

	snprintf(name, PATH_MAX, "%s%s%s", kvm__get_dir(), guestfs_name, path);
	snprintf(target, PATH_MAX, "/host%s", path);

	return symlink(target, name);
}

static int make_dir(const char *dir)
{
	char name[PATH_MAX];
	snprintf(name, PATH_MAX, "%s%s", kvm__get_dir(), dir);
	return mkdir(name, 0777);
}

static void make_guestfs_dir(const char *guestfs_name, const char *dir)
{
	char name[PATH_MAX];
	snprintf(name, PATH_MAX, "%s%s", guestfs_name, dir);
	make_dir(name);
}

void kvm_setup_resolv(const char *guestfs_name)
{
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s%s/etc/resolv.conf", kvm__get_dir(), guestfs_name);
	copy_file("/etc/resolv.conf", path);
}

int kvm_setup_create_new(const char *guestfs_name)
{
	unsigned int i;
	int ret;

	ret = make_dir(guestfs_name);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(guestfs_dirs); i++)
		make_guestfs_dir(guestfs_name, guestfs_dirs[i]);

	for (i = 0; i < ARRAY_SIZE(guestfs_symlinks); i++)
		make_guestfs_symlink(guestfs_name, guestfs_symlinks[i]);

	ret = kvm_setup_guest_init(guestfs_name);
	if (ret < 0)
		return ret;

	return copy_passwd(guestfs_name);
}

int kvmm_cmd_setup(int argc, const char **argv)
{
	const char *new_guest = "default";

	if (argc == 1)
		new_guest = argv[0];
	else if (argc > 1) {
		printf("  \033[1;33m[WARN]\033[0m Only ONE guest at a time.\n");
		return -1;
	}
		
	int r = kvm_setup_create_new(new_guest);
	if (r != 0)
		printf("Unable to create rootfs in %s%s: %s\n", kvm__get_dir(), new_guest, strerror(errno));
	else
		printf("A new rootfs '%s' has been created in '%s%s'.\n\n"
			"You can now start it by running the following command:\n\n  %s start -d %s\n",
			new_guest, kvm__get_dir(), new_guest, KVMM_NAME, new_guest);
	return r;
}
