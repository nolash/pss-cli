#include <stdlib.h>
#include <string.h>

#include "cmd.h"

psscli_cmd* psscli_cmd_alloc(psscli_cmd *cmd, int valuecount) {
	cmd->valuecount = valuecount;
	cmd->values = malloc(sizeof(char*)*cmd->valuecount);
	if (cmd->values == NULL) {
		cmd->valuecount = 0;
		return NULL;
	}
	return cmd;
}

void psscli_cmd_free(psscli_cmd *cmd) {
	int i;
	if (cmd->values != NULL)  {
		for (i = 0; i < cmd->valuecount; i++) {
			if (cmd->values+i != NULL) {
				free(*(cmd->values+i));
			}
		}
		free(cmd->values);
	}
	memset(&cmd, 0, sizeof(psscli_cmd));
}

