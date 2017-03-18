#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kvmc.h>

static int kvmc_exec(const char *cmd, int argc, const char **argv)
{
	int ret = 0;
	struct kvmc_cmd_s *p = kvmc_get_cmd(cmd);
	if (p == NULL)
		return EINVAL;

	ret = p->exec(argc, argv);
	if ((ret < 0) && (errno == EPERM))
		printf("  \033[1;33m[WARN]\033[0m Permission denied, maybe you're not root.\n");
	return ret;
}

int main(int argc, char *argv[])
{
	int retval = EXIT_SUCCESS;
	char *input, kvmc_prompt[KVMC_PROMPT_MAX_LEN];

	retval = mkdir(KVMC_ROOT, 0777);
	if (retval && (errno != EEXIST)) {
		printf("  \033[1;33m[WARN]\033[0m %s exists, but is not an accessible directory.\n", KVMC_ROOT);
	}
	if (argc > 1)
		return kvmc_exec(argv[1], argc - 2, (const char **)&argv[2]);

	// Configure readline to auto-complete paths when the tab key is hit.
	// TODO: auto-complete on subcommands
	rl_bind_key('\t', rl_complete);

	while(true) {
		snprintf(kvmc_prompt, sizeof(kvmc_prompt), "[KVMC( %s )]>> ", getenv("USER"));
		input = readline(kvmc_prompt);
		if (strlen(input) == 0)
			continue;
		else if ((!strncmp(input, "exit", 5)) || (!strncmp(input, "quit", 5)))
			break;
		add_history(input);

		int fake_c = 0;
		char *fake_v[KVMC_MAX_OPTS + 1] = {NULL};
		char *parse_loc = input;
		char *parse_idx = NULL;
		while (parse_loc && (fake_c < KVMC_MAX_OPTS)) {
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

		kvmc_exec(input, fake_c, (const char **)fake_v);
		free(input);
	}
	return retval;
}
