#include <linux/types.h>

#define	KVMM_NAME		"kvmm"

/* Version: "v" + Major + Minor [+ <d(raft)|a(lpha)|b(eta)>] */
#define KVMM_VERSION		"v0.1.d"

/* A subcommand in kvmm should be relatively short */
#define KVMM_CMD_MAX_LEN	16
#define KVMM_PROMPT_MAX_LEN	32
#define KVMM_MAX_OPTS		16
#define KVM_DEBUG_CMD_TYPE_DUMP (1 << 0)
#define KVM_DEBUG_CMD_TYPE_NMI  (1 << 1)
#define KVM_DEBUG_CMD_TYPE_SYSRQ (1 << 2)

#define KVMM_GUEST_PRE_INIT	"guest/pre_init"
#define KVMM_GUEST_INIT		"guest/init"

struct debug_cmd_params {
	u32 dbg_type;
	u32 cpu;
	char sysrq;
};

struct kvmm_cmd_s {
	char *name;
	int (*exec)(int argc, const char **argv);
	void (*help)(void);
	char *desc;
};

/*
 * Redirect to kvmm subcommand according to argv[1]
 *
 * argv of a subcommand starts from argv[2] (if exists)
 */
struct kvmm_cmd_s *kvmm_get_cmd(const char *cmd);

/* Prints version string, argc and argv are not used */
int kvmm_cmd_version(int argc, const char **argv);

int kvmm_cmd_help(int argc, const char **argv);
int kvmm_cmd_stat(int argc, const char **argv);
int kvmm_cmd_stop(int argc, const char **argv);
int kvmm_cmd_pause(int argc, const char **argv);
int kvmm_cmd_resume(int argc, const char **argv);
int kvmm_cmd_balloon(int argc, const char **argv);
int kvmm_cmd_debug(int argc, const char **argv);
int kvmm_cmd_ls(int argc, const char **argv);
int kvmm_cmd_start(int argc, const char **argv);
int kvmm_cmd_setup(int argc, const char **argv);

void kvmm_help_start(void);
void kvmm_help_setup(void);
void kvmm_help_ls(void);
void kvmm_help_debug(void);
void kvmm_help_stop(void);
void kvmm_help_stat(void);
void kvmm_help_pause(void);
void kvmm_help_resume(void);
void kvmm_help_balloon(void);

int get_vmstate(int sock);
int kvm_setup_create_new(const char *guestfs_name);
void kvm_setup_resolv(const char *guestfs_name);
int kvm_setup_guest_init(const char *guestfs_name);
