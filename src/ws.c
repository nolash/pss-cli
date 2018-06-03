#include <libwebsockets.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <pthread.h>

#include "cmd.h"
#include "ws.h"
#include "sync.h"
#include "error.h"
#include "extra.h"
#include "config.h"

// setup
extern struct psscli_config conf;

// ws 
struct lws_protocols psscli_protocols_[] = {
	{ PSSCLI_WS_PROTOCOL_NAME, NULL, 0, PSSCLI_WS_RX_BUFFER },
	{ NULL, NULL, 0, 0}
};
extern struct psscli_ws_ psscli_ws;

// sync controls
extern pthread_t pt_parse;
extern pthread_t pt_write;
extern pthread_cond_t pt_cond_parse;
extern pthread_cond_t pt_cond_write;
extern pthread_mutex_t pt_lock_state;
extern pthread_mutex_t pt_lock_queue;
static pthread_mutex_t pt_lock_parse;
static pthread_mutex_t pt_lock_write;
extern pthread_cond_t pt_cond_reply;

// queues
extern psscli_cmd psscli_cmd_current;

// internal functions
static int psscli_ws_write_(struct lws *ws, char *buf, int buflen, enum lws_write_protocol wp);
static int running();
PRIVATE int json_write(char *json_string, int json_string_len, psscli_cmd *cmd);
PRIVATE int json_parse(psscli_cmd *cmd);
json_object *j_version;
void psscli_ws_connect_try_(int s);

void psscli_ws_connect_try_(int s) {
	pthread_mutex_lock(&pt_lock_state);
	psscli_ws.connected = 0;
	pthread_mutex_unlock(&pt_lock_state);
	lws_client_connect_via_info(&psscli_ws.wi);
	if (s > 0) {
		sleep(1);
	}
}

// set up websocket and ipc
int psscli_ws_init(psscli_ws_callback callback, const char *version) {
	struct sigaction sa;

	psscli_protocols_[0].callback = (void*)callback;
	//psscli_protocols_[0].user = (void*)&psscli_cmd_current;
	memset(&(psscli_ws.wci), 0, sizeof(psscli_ws.wci));
	psscli_ws.wci.uid = -1;
	psscli_ws.wci.gid = -1;
	psscli_ws.wci.port = CONTEXT_PORT_NO_LISTEN;
	psscli_ws.wci.protocols = psscli_protocols_;

	psscli_ws.ctx = lws_create_context(&(psscli_ws.wci));
	if (psscli_ws.ctx == NULL) {
		return 1;
	}

	memset(&(psscli_ws.wi), 0, sizeof(psscli_ws.wi));
	psscli_ws.wi.context = psscli_ws.ctx;
	psscli_ws.wi.address = conf.host;
	psscli_ws.wi.port = conf.port;
	psscli_ws.wi.host = conf.host;
	psscli_ws.wi.origin = conf.host;
	psscli_ws.wi.protocol = PSSCLI_WS_PROTOCOL_NAME;
	psscli_ws.wi.path = "/";
	psscli_ws.wi.pwsi = &(psscli_ws.w);
	psscli_ws.wi.ietf_version_or_minus_one = -1;

	psscli_ws.pid = getpid();

	j_version = json_object_new_string(version);

	sa.sa_handler = psscli_ws_connect_try_;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);

	return 0;
}

static int parse(psscli_cmd *cmd) {
	if (json_parse(cmd)) {
		return 1;
	}
	return 0;
}

void *parse_loop(void *arg) {
	int c;
	int last;
	psscli_cmd *p;
	psscli_cmd cmd;

	//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	fprintf(stderr, "entering parseloop\n");
	while (running()) {
		pthread_mutex_lock(&pt_lock_parse);
		pthread_cond_wait(&pt_cond_parse, &pt_lock_parse);
		fprintf(stderr, "parseloop wakeup\n");
		pthread_mutex_unlock(&pt_lock_parse);
		if (!running()) {
			fprintf(stderr, "parseloop exit\n");
			break;
		}
		while(1) {
			pthread_mutex_lock(&pt_lock_queue);
			p = psscli_cmd_queue_peek(PSSCLI_QUEUE_IN);
			if (p == NULL) {
				pthread_mutex_unlock(&pt_lock_queue);
				break;
			}
			psscli_cmd_copy(&cmd, p);
			pthread_mutex_unlock(&pt_lock_queue);
			if (parse(&cmd)) {
				break;
			}
			pthread_mutex_lock(&pt_lock_queue);
			psscli_cmd_copy(p, &cmd);
			pthread_mutex_unlock(&pt_lock_queue);
			pthread_cond_signal(&pt_cond_reply);
		}
	}

	pthread_exit(NULL);
	return NULL;
}

// \todo put json write in this loop
void *write_loop(void *arg) {
	int last;
	int next;
	psscli_cmd *cmd;

	//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	fprintf(stderr, "entering writeloop\n");
	while (running()) {
		pthread_mutex_lock(&pt_lock_write);
		pthread_cond_wait(&pt_cond_write, &pt_lock_write);
		fprintf(stderr, "writeloop wakeup\n");
		pthread_mutex_unlock(&pt_lock_write);
		if (!running()) {
			fprintf(stderr, "writeloop exit\n");
			break;
		}
		pthread_mutex_lock(&pt_lock_queue);
		cmd = psscli_cmd_queue_peek(PSSCLI_QUEUE_OUT);
		if (cmd == NULL) {
			pthread_mutex_unlock(&pt_lock_queue);
			continue;
		}

		// set the next command to be sent only if:
		// * we don't have a current command, OR
		// * the current command is still pending
		if (cmd->code == PSSCLI_CMD_NONE) {
			pthread_mutex_unlock(&pt_lock_queue);
			fprintf(stderr, "next is noop");
			continue;
		}
		// \todo this should be set only after json formatting is completed
		cmd->status |= PSSCLI_STATUS_VALID; 
		pthread_mutex_unlock(&pt_lock_queue);

		// if command is pending, then calling writable will try to dispatch it again
		lws_callback_on_writable(psscli_ws.w);
	}

	pthread_exit(NULL);
	return NULL;
}

int psscli_ws_start() {
	int r;

	//lws_client_connect_via_info(&psscli_ws.wi);
	psscli_ws_connect_try_(0);
	r = pthread_create(&pt_parse, NULL, parse_loop, NULL);
	if (r) {
		fprintf(stderr, "parse thread create fail\n");
		return PSSCLI_EINIT;
	}
	r = pthread_create(&pt_write, NULL, write_loop, NULL);
	if (r) {
		fprintf(stderr, "write thread create fail\n");
		return PSSCLI_EINIT;
	}
	while (running()) {
		fprintf(stderr, "poll ws\n");
		lws_service(psscli_ws.ctx, PSSCLI_WS_LOOP_TIMEOUT);
	}
	pthread_cond_broadcast(&pt_cond_write);
	pthread_cond_broadcast(&pt_cond_parse);
	pthread_join(pt_write, NULL);
	pthread_join(pt_parse, NULL);
	fprintf(stderr, "connect loop exiting\n");
	return PSSCLI_EOK;
}

static int running() {
	int r;

	pthread_mutex_lock(&pt_lock_state);
	r = psscli_ws.pid;
	pthread_mutex_unlock(&pt_lock_state);

	return r;
}

// \todo orderly catch both threads
void psscli_ws_stop() {
	if (psscli_ws.ctx != NULL) {
		lws_context_destroy(psscli_ws.ctx);
	}
	pthread_mutex_lock(&pt_lock_state);
	psscli_ws.pid = 0;
	psscli_ws.connected = 0;	
	pthread_mutex_unlock(&pt_lock_state);
}

int psscli_ws_send(psscli_cmd *cmd) {
	char in_buf[1024];
	int r;
	json_object *j;

	r = json_write(in_buf + LWS_PRE, 1024 - LWS_PRE, cmd);
	if (r) {
		return 2;
	}

	fprintf(stderr, "sending: %s\n", &in_buf[LWS_PRE]);
	r = psscli_ws_write_(psscli_ws.w, &in_buf[LWS_PRE], strlen(&in_buf[LWS_PRE]), LWS_WRITE_TEXT);
	if (r < 0) {
		return 3;
	}

	return 0;
}

int psscli_ws_ready() {
	int r;

	pthread_mutex_lock(&pt_lock_state);
	r = psscli_ws.connected;
	pthread_mutex_unlock(&pt_lock_state);

	return r;
}
	

// wrapper for lws_write to increment rcp call id after successful calls
int psscli_ws_write_(struct lws *ws, char *buf, int buflen, enum lws_write_protocol wp) {
	int r;
	r = lws_write(ws, buf, buflen, wp);
	if (r > 0) {
		psscli_ws.id++;
	}
	return r;
}

int json_parse(psscli_cmd *cmd) {
	json_tokener *jt;
	json_object *j;
	json_object *jv;
	const char *p;

	if (!(cmd->status & PSSCLI_STATUS_COMPLETE)) {
		return PSSCLI_ENODATA;
	}

	jt = json_tokener_new();
	j = json_tokener_parse_ex(jt, cmd->src, cmd->srclength);
	if (j == NULL) {
		return PSSCLI_EINVAL;
	}
	json_tokener_free(jt);

	if (!json_object_object_get_ex(j, "jsonrpc", &jv)) {
		return PSSCLI_EINVAL;
	} else if (strcmp(json_object_get_string(jv), json_object_get_string(j_version))) {
		return PSSCLI_EINVAL;
	}

	if(!json_object_object_get_ex(j, "id", &jv)) {
		cmd->status &= (~PSSCLI_STATUS_VALID);
		return PSSCLI_EINVAL;
	}
	cmd->id = strtol(json_object_get_string(jv), NULL, 10);
	if (errno) {
		cmd->status &= (~PSSCLI_STATUS_VALID);
		return PSSCLI_EINVAL;
	}
	if (!json_object_object_get_ex(j, "result", &jv)) {
		cmd->status &= (~PSSCLI_STATUS_VALID);
		return PSSCLI_EINVAL;
	}
	if (cmd->values != NULL) {
		free(*(cmd->values));
	} else {
		cmd->values = malloc(sizeof(char**));
	}
	p = json_object_get_string(jv);
	*(cmd->values) = malloc(sizeof(char)*strlen(p));
	strcpy(*(cmd->values), p);

	return PSSCLI_EOK;
}

// create json rpc string from cmd 
int json_write(char *json_string, int json_string_len, psscli_cmd *cmd) {
	json_object *j;
	json_object *jp;
	json_object *jv;
	char method[64];
	char data[1024];

	jp = NULL;
	j = json_object_new_object();
	json_object_object_add(j, "jsonrpc", j_version);

	switch(cmd->code) {
		case PSSCLI_CMD_BASEADDR:
			strcpy(method, "pss_baseAddr");
			break;
		case PSSCLI_CMD_GETPUBLICKEY:
			strcpy(method, "pss_getPublicKey");
			break;
		case PSSCLI_CMD_SETPEERPUBLICKEY:
			strcpy(method, "pss_setPeerPublicKey");
			jp = json_object_new_array();
			jv = json_object_new_string(*(cmd->values));
			json_object_array_add(jp, jv);
			jv = json_object_new_string(*(cmd->values+1));
			json_object_array_add(jp, jv);
			if (*(cmd->values+2) != NULL) {
				jv = json_object_new_string(*(cmd->values+2));
			} else {
				jv = json_object_new_string("0x");
			}
			json_object_array_add(jp, jv);
			psscli_cmd_free(cmd);
			break;
		default: 
			return 1;

	}
	jv = json_object_new_int64(psscli_ws.id);
	json_object_object_add(j, "id", jv);
	jv = json_object_new_string(method);
	json_object_object_add(j, "method", jv);
	if (jp != NULL) {
		json_object_object_add(j, "params", jp);
	}
	strcpy(json_string, json_object_to_json_string_ext(j, JSON_C_TO_STRING_PLAIN));
	return 0;
}
