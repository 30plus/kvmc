#include <linux/types.h>

#define	KVMC_NAME		"kvmc"

/* Version: "v" + Major + Minor [+ <d(raft)|a(lpha)|b(eta)>] */
#define KVMC_VERSION		"v0.1.d"

/* A subcommand in kvmc should be relatively short */
#define KVMC_CMD_MAX_LEN	16
#define KVMC_PROMPT_MAX_LEN	32
#define KVMC_MAX_OPTS		16
#define KVM_DEBUG_CMD_TYPE_DUMP (1 << 0)
#define KVM_DEBUG_CMD_TYPE_NMI  (1 << 1)
#define KVM_DEBUG_CMD_TYPE_SYSRQ (1 << 2)

#define KVMC_GUEST_PRE_INIT	"util/guest/pre_init"
#define KVMC_GUEST_INIT		"util/guest/init"

struct debug_cmd_params {
	u32 dbg_type;
	u32 cpu;
	char sysrq;
};

struct kvmc_cmd_s {
	const char *const name;
	int (* const exec)(int argc, const char **argv);
	void (* const help)(void);
	const char * const desc;
};

/*
 * Redirect to kvmc subcommand according to argv[1]
 *
 * argv of a subcommand starts from argv[2] (if exists)
 */
struct kvmc_cmd_s *kvmc_get_cmd(const char *cmd);

/* Prints version string, argc and argv are not used */
int kvmc_cmd_version(int argc, const char **argv);

int kvmc_cmd_help(int argc, const char **argv);
int kvmc_cmd_stat(int argc, const char **argv);
int kvmc_cmd_stop(int argc, const char **argv);
int kvmc_cmd_pause(int argc, const char **argv);
int kvmc_cmd_resume(int argc, const char **argv);
int kvmc_cmd_balloon(int argc, const char **argv);
int kvmc_cmd_debug(int argc, const char **argv);
int kvmc_cmd_ls(int argc, const char **argv);
int kvmc_cmd_start(int argc, const char **argv);
int kvmc_cmd_setup(int argc, const char **argv);

void kvmc_help_start(void);
void kvmc_help_setup(void);
void kvmc_help_ls(void);
void kvmc_help_debug(void);
void kvmc_help_stop(void);
void kvmc_help_stat(void);
void kvmc_help_pause(void);
void kvmc_help_resume(void);
void kvmc_help_balloon(void);

int get_vmstate(int sock);
int kvm_setup_create_new(const char *guestfs_name);
void kvm_setup_resolv(const char *guestfs_name);
int kvm_setup_guest_init(const char *guestfs_name);
