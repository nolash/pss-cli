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

// queues
extern psscli_cmd psscli_cmd_current;

// internal functions
static int psscli_ws_write_(struct lws *ws, char *buf, int buflen, enum lws_write_protocol wp);
static int psscli_ws_json_(char *json_string, int json_string_len, psscli_cmd *cmd);
json_object *j_version;
void psscli_ws_connect_try_(int s);

void psscli_ws_connect_try_(int s) {
	psscli_ws.connected = 0;
	lws_client_connect_via_info(&psscli_ws.wi);
	if (s > 0) {
		sleep(1);
	}
}

// set up websocket and ipc
int psscli_ws_init(psscli_ws_callback callback, const char *version) {
	struct sigaction sa;

	psscli_protocols_[0].callback = (void*)callback;
	psscli_protocols_[0].user = (void*)&psscli_cmd_current;
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
	psscli_ws.wi.address = psscli_ws.host;
	psscli_ws.wi.port = psscli_ws.port;
	psscli_ws.wi.host = psscli_ws.host;
	psscli_ws.wi.origin = psscli_ws.origin;
	psscli_ws.wi.protocol = PSSCLI_WS_PROTOCOL_NAME;
	psscli_ws.wi.path = "/";
	psscli_ws.wi.pwsi = &(psscli_ws.w);
	psscli_ws.wi.ietf_version_or_minus_one = -1;

	// we use pipe to insert into the send queue between command unix socket and websocket
	if (pipe(psscli_ws.notify) < 0) {
		return 1;
	}
	fcntl(psscli_ws.notify[0], F_SETFL, O_NONBLOCK);

	psscli_ws.pid = getpid();

	j_version = json_object_new_string(version);

	sa.sa_handler = psscli_ws_connect_try_;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);

	return 0;
}

static int parse(psscli_response *r) {
	fprintf(stderr, "got response: %s\n", r);
	return 0;
}

static void *parse_loop() {
	int run;
	int c;
	int last;
	psscli_response *p;
	psscli_response response;

	fprintf(stderr, "entering parseloop\n");
	pthread_mutex_lock(&pt_lock_state);
	run = psscli_ws.pid;
	pthread_mutex_unlock(&pt_lock_state);
	while (run) {
		pthread_mutex_lock(&pt_lock_state);
		pthread_cond_wait(&pt_cond_parse, &pt_lock_state);
		run = psscli_ws.pid;
		if (!run) {
			pthread_mutex_unlock(&pt_lock_state);
			fprintf(stderr, "parseloop exit\n");
			pthread_exit(&pt_parse);
			break;
		}
		while(1) {
			pthread_mutex_lock(&pt_lock_queue);
			p = psscli_response_queue_next();
			if (p == NULL) {
				pthread_mutex_unlock(&pt_lock_queue);
				break;
			}
			memcpy(&response, p, sizeof(response));
			pthread_mutex_unlock(&pt_lock_queue);
			if (parse(&response)) {
				break;
			}
		}
	}

	return NULL;
}

static void *write_loop() {
	int run;
	int last;
	int next;
	psscli_cmd *cmd;

	fprintf(stderr, "entering writeloop\n");
	pthread_mutex_lock(&pt_lock_state);
	run = psscli_ws.pid;
	pthread_mutex_unlock(&pt_lock_state);
	while (run) {
		pthread_mutex_lock(&pt_lock_state);
		pthread_cond_wait(&pt_cond_write, &pt_lock_state);
		fprintf(stderr, "writeloop wakeup\n");
		run = psscli_ws.pid;
		if (!run) {
			pthread_mutex_unlock(&pt_lock_state);
			fprintf(stderr, "writeloop exit\n");
			pthread_exit(&pt_write);
			break;
		}
		pthread_mutex_unlock(&pt_lock_state);
		pthread_mutex_lock(&pt_lock_queue);
		cmd = psscli_cmd_queue_next();
		if (cmd == NULL) {
			pthread_mutex_unlock(&pt_lock_queue);
			continue;
		}

		// set the next command to be sent only if:
		// * we don't have a current command, OR
		// * the current command is still pending
		if (cmd->code == PSSCLI_CMD_NONE) {
			fprintf(stderr, "next is noop");
			continue;
		}
		if (psscli_cmd_copy(&psscli_cmd_current, cmd)) {
			fprintf(stderr, "cmd copy fail in write thread\n");
			continue;
		}
		pthread_mutex_unlock(&pt_lock_queue);

		// if command is pending, then calling writable will try to dispatch it again
		lws_callback_on_writable(psscli_ws.w);
	}

	return NULL;
}


/***
 * \short connects to websocket on swarm (pss) node
 * \description polls for servicing the websocket and reads from command unix socket pipe every turn
 * \todo retry loop on connect fail
 */ 
int psscli_ws_connect() {
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
	while (psscli_ws.pid) {
		fprintf(stderr, "poll ws\n");
		lws_service(psscli_ws.ctx, PSSCLI_WS_LOOP_TIMEOUT);
	}
	pthread_cond_signal(&pt_cond_write);
	pthread_cond_signal(&pt_cond_parse);
	pthread_join(pt_write, NULL);
	pthread_join(pt_parse, NULL);
	fprintf(stderr, "connect loop exiting\n");
	return PSSCLI_EOK;
}

void psscli_ws_free() {
	if (psscli_ws.ctx != NULL) {
		lws_context_destroy(psscli_ws.ctx);
	}
}

/***
 * \short send command on websocket
 * \description converts command structure to json rpc and sends to websocket. Called from callback when websocket is writable
 * \todo fail handling
 */
int psscli_ws_send(psscli_cmd *cmd) {
	char in_buf[1024];
	int r;
	json_object *j;

	r = psscli_ws_json_(in_buf + LWS_PRE, 1024 - LWS_PRE, cmd);
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

// wrapper for lws_write to increment rcp call id after successful calls
int psscli_ws_write_(struct lws *ws, char *buf, int buflen, enum lws_write_protocol wp) {
	int r;
	r = lws_write(ws, buf, buflen, wp);
	if (r > 0) {
		psscli_ws.id++;
	}
	return r;
}

// create json rpc string from cmd 
int psscli_ws_json_(char *json_string, int json_string_len, psscli_cmd *cmd) {
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
			if ((cmd->values+2) != NULL) {
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
