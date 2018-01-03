#include <libwebsockets.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <stdlib.h>

#include "ws.h"

extern struct psscli_ws_ psscli_ws;

struct lws_protocols psscli_protocols_[] = {
	{ PSSCLI_WS_PROTOCOL_NAME, NULL, 0, PSSCLI_WS_RX_BUFFER },
	{ NULL, NULL, 0, 0}
};

int psscli_ws_write_(struct lws *ws, char *buf, int buflen, enum lws_write_protocol wp);
int psscli_ws_json_(char *json_string, int json_string_len, enum psscli_cmd_code code, int cargc, void **cargv);

json_object *j_version;

int psscli_ws_init(psscli_ws_callback callback, const char *version) {
	psscli_protocols_[0].callback = (void*)callback;
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

	// use pipe to insert into the send queue between command unix socket and websocket
	if (pipe(psscli_ws.notify) < 0) {
		return 1;
	}
	fcntl(psscli_ws.notify[0], F_SETFL, O_NONBLOCK);

	psscli_ws.pid = getpid();

	j_version = json_object_new_string(version);

	return 0;
}

// connects to websocket on swarm (pss) node
// polls for servicing the websocket and reads from command unix socket pipe every turn
void *psscli_ws_connect(void *v) {
	char n;
	lws_client_connect_via_info(&psscli_ws.wi);
	while (psscli_ws.pid) {
		printf("poll\n");
		lws_service(psscli_ws.ctx, PSSCLI_WS_LOOP_TIMEOUT);
		if (read(psscli_ws.notify[0], &n, 1) > 0) {
			lws_callback_on_writable(psscli_ws.w);
		}
	}
}

void psscli_ws_free() {
	if (psscli_ws.ctx != NULL) {
		lws_context_destroy(psscli_ws.ctx);
	}
}

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

int psscli_ws_json_(char *json_string, int json_string_len, psscli_cmd *cmd) {
	json_object *j;
	json_object *j_id;
	json_object *j_method;
	char method[64];

	j = json_object_new_object();
	json_object_object_add(j, "jsonrpc", j_version);

	switch(cmd->code) {
		case PSSCLI_CMD_BASEADDR:
			strcpy(method, "pss_baseAddr");
			break;
		case PSSCLI_CMD_SETPEERPUBLICKEY:
			strcpy(method, "pss_setPeerPublickey");
			break;
		default: 
			return 1;

	}

	j_id = json_object_new_int64(psscli_ws.id);
	json_object_object_add(j, "id", j_id);
	j_method = json_object_new_string(method);
	json_object_object_add(j, "method", j_method);
	strcpy(json_string, json_object_to_json_string_ext(j, JSON_C_TO_STRING_PLAIN));
	return 0;
}
