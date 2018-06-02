#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "error.h"
#include "minq.h"

// state
static char started;

// outgoing messages queues for websocket
queue_t cmd_queue;
queue_t response_queue;

// incremented on each queued message
static int seq;

int psscli_queue_start(short cmdsize, short responsesize) {
	minq_init(&cmd_queue, cmdsize);
	minq_init(&response_queue, responsesize);
	psscli_cmd_current.status = PSSCLI_CMD_STATUS_NONE;
	psscli_cmd_current.valuecount = 0;
	psscli_response_current.status = PSSCLI_RESPONSE_STATUS_NONE;
	psscli_response_current.length = 0;
	started = 1;
	seq = 0;
	return PSSCLI_EOK;
}

void psscli_queue_stop() {
	minq_free(&cmd_queue);	
	minq_free(&response_queue);
}

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
	if (cmd == NULL) {
		return;
	}
	if (cmd->values != NULL)  {
		for (i = 0; i < cmd->valuecount; i++) {
			if (cmd->values+i != NULL) {
				free(*(cmd->values+i));
			}
		}
		free(cmd->values);
	}
	memset(cmd, 0, sizeof(psscli_cmd));
}

int psscli_cmd_copy(psscli_cmd *to, psscli_cmd *from) {
	int i, j;

	for (i = 0; i < to->valuecount; i++) {
		free(*(to->values+i));
	}
	free(to->values);
	to->values = malloc(sizeof(char**)*from->valuecount);
	if (to->values == NULL) {
		return PSSCLI_EMEM;
	}
	for (i = 0; i < from->valuecount; i++) {
		*(to->values+i) = malloc(sizeof(char)*strlen(*(from->values)+i));
		if (*(to->values+i) == NULL) {
			for (j = i-1; j >= 0; j--) {
				free(*(to->values+j));
			}
			free(to->values);
			return PSSCLI_EMEM;
		}
		strcpy(*(to->values+i), *(from->values+i));
	}
	to->valuecount = from->valuecount;
	to->id = from->id;
	to->code = from->code;
	to->status = from->status;
	return PSSCLI_EOK;
}

int psscli_cmd_queue_add(psscli_cmd *cmd) {
	if (minq_add(&cmd_queue, (void*)cmd) == -1) {
		return -1;
	}
	cmd->id = seq++;
	return 0;
}

psscli_cmd *psscli_cmd_queue_peek() {
	void *entry;
	psscli_cmd *cmd;

	if (minq_peek(&cmd_queue, &entry)) {
		return NULL;
	}
	cmd = (psscli_cmd*)entry;
	if (cmd->code == PSSCLI_CMD_NONE) {
		return NULL;
	}
	return cmd;
}

psscli_cmd *psscli_cmd_queue_next() {
	void *entry;
	psscli_cmd *cmd;

	if (minq_next(&cmd_queue, &entry)) {
		return NULL;
	}
	cmd = (psscli_cmd*)entry;
	if (cmd->code == PSSCLI_CMD_NONE) {
		return NULL;
	}
	return cmd;
}

int psscli_response_queue_add(psscli_response *response) {
	if (minq_add(&response_queue, (void*)response) == -1) {
		return -1;
	}
	return 0;
}

psscli_response *psscli_response_queue_peek() {
	void *entry;
	psscli_response *response;

	if (minq_peek(&response_queue, &entry)) {
		return NULL;
	}
	return (psscli_response*)entry;
}

psscli_response *psscli_response_queue_next() {
	void *entry;
	psscli_response *response;

	if (minq_next(&response_queue, &entry)) {
		return NULL;
	}
	return (psscli_response*)entry;
}
