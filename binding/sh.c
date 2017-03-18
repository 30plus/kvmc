#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "kvm/kvm.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <kvmm.h>

int kvmm_exec(const char *cmd, int argc, const char **argv)
{
	int ret = 0;
	struct kvmm_cmd_s *p = kvmm_get_cmd(cmd);
	if (p == NULL)
		return EINVAL;

	ret = p->exec(argc, argv);
	if ((ret < 0) && (errno == EPERM))
			die("Permission error - are you root?");
	return ret;
}

int main(int argc, char *argv[])
{
	int retval = EXIT_SUCCESS;
	char *input, kvmm_prompt[KVMM_PROMPT_MAX_LEN];
	int (*exec)(int argc, char **argv) = NULL;

	kvm__set_dir("%s/%s", HOME_DIR, KVM_PID_FILE_PATH);
	if (argc > 1)
		return kvmm_exec(argv[1], argc - 2, (const char **)&argv[2]);

	// Configure readline to auto-complete paths when the tab key is hit.
	// TODO: auto-complete on subcommands
	rl_bind_key('\t', rl_complete);

	for(;;) {
		snprintf(kvmm_prompt, sizeof(kvmm_prompt), "[KVMM( %s )]>> ", getenv("USER"));
		input = readline(kvmm_prompt);
		if (strlen(input) == 0)
			continue;
		else if ((!strncmp(input, "exit", 5)) || (!strncmp(input, "quit", 5)))
			break;
		add_history(input);

		int fake_c = 0;
		char *fake_v[KVMM_MAX_OPTS + 1] = {NULL};
		char *parse_loc = input;
		char *parse_idx = NULL;
		while (parse_loc && (fake_c < KVMM_MAX_OPTS)) {
			if (!(parse_idx = strchr(parse_loc, ' ')) && !(parse_idx = strchr(parse_loc, '\t')))
				break;

			while ((parse_idx[0] == ' ') || (parse_idx[0] == '\t')) {
				parse_idx[0] = 0;
				parse_idx++;
			}

			if (parse_idx[0] != 0) {
				fake_v[fake_c] = parse_loc = parse_idx;
				fake_c++;
			} else
				break;
		}
		fake_v[fake_c] = 0;

		kvmm_exec(input, fake_c, (const char **)fake_v);	// TODO: Parse input as (argc,argv)
		free(input);
	}
	return retval;
}
