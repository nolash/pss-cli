#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "error.h"
#include "minq.h"

// state
static char started;

// message queues for the system
// 0 = outgoing from socket
// 1 = in websocket transfer
// 2 = incoming to socket (both sync replies and subscriptions)
queue_t q[3];

// incremented on each queued message
static int seq;

int psscli_queue_start(short size) {
	minq_init(&q[PSSCLI_QUEUE_OUT], size);
	minq_init(&q[PSSCLI_QUEUE_X], size);
	minq_init(&q[PSSCLI_QUEUE_IN], size);
	started = 1;
	seq = 0;
	return PSSCLI_EOK;
}

void psscli_queue_stop() {
	minq_free(&q[PSSCLI_QUEUE_OUT]);	
	minq_free(&q[PSSCLI_QUEUE_X]);
	minq_free(&q[PSSCLI_QUEUE_IN]);
}

psscli_cmd* psscli_cmd_alloc(psscli_cmd **cmd, int valuecount) {
	int i;
	psscli_cmd *p;

	p = malloc(sizeof(psscli_cmd));
	p->status = 0;
	p->valuecount = valuecount;
	p->values = malloc(sizeof(char**)*valuecount);
	if (p->values == NULL && valuecount > 0) {
		p->valuecount = 0;
		return NULL;
	}
	for (i = 0; i < valuecount; i++) {
		*(p->values) = NULL;
	}
	p->id = -1;
	p->sd = -1;
	p->sdptr = 0;
	p->code = PSSCLI_CMD_NONE; 
	memset(p->src, 0, sizeof(p->src));
	*cmd = p;
	return *cmd;
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
	free(cmd);
}

int psscli_cmd_copy(psscli_cmd *to, psscli_cmd *from) {
	int i, j;

	for (i = 0; i < to->valuecount; i++) {
		if (*(to->values+1) != NULL) {
			free(*(to->values+i));
		}
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
	to->sd = from->sd;
	to->sdptr = from->sdptr;
	memcpy(to->src, from->src, sizeof(from->src));
	return PSSCLI_EOK;
}

int psscli_cmd_queue_add(char qid, psscli_cmd *cmd) {
	if (minq_add(&q[qid], (void*)cmd) == -1) {
		return -1;
	}
	cmd->id = seq++;
	return 0;
}

psscli_cmd *psscli_cmd_queue_peek(char qid) {
	void *entry;
	psscli_cmd *cmd;

	if (minq_peek(&q[qid], &entry)) {
		return NULL;
	}
	cmd = (psscli_cmd*)entry;
	return cmd;
}

psscli_cmd *psscli_cmd_queue_next(char qid) {
	void *entry;
	psscli_cmd *cmd;

	if (minq_next(&q[qid], &entry)) {
		return NULL;
	}
	return (psscli_cmd*)entry;
}

//int psscli_response_queue_add(psscli_response *response) {
//	if (minq_add(&response_queue, (void*)response) == -1) {
//		return -1;
//	}
//	return 0;
//}
//
//psscli_response *psscli_response_queue_peek() {
//	void *entry;
//	psscli_response *response;
//
//	if (minq_peek(&response_queue, &entry)) {
//		return NULL;
//	}
//	return (psscli_response*)entry;
//}
//
//psscli_response *psscli_response_queue_next() {
//	void *entry;
//	psscli_response *response;
//
//	if (minq_next(&response_queue, &entry)) {
//		return NULL;
//	}
//	return (psscli_response*)entry;
//}
