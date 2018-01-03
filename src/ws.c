#include <libwebsockets.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "ws.h"

extern struct psscli_ws_ psscli_ws;

struct lws_protocols psscli_protocols_[] = {
	{ PSSCLI_WS_PROTOCOL_NAME, NULL, 0, PSSCLI_WS_RX_BUFFER },
	{ NULL, NULL, 0, 0}
};

int psscli_ws_init(psscli_ws_callback callback) {
	//struct lws_context_creation_info wci;

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

	if (pipe(psscli_ws.notify) < 0) {
		return 1;
	}
	fcntl(psscli_ws.notify[0], F_SETFL, O_NONBLOCK);

	psscli_ws.pid = getpid();

	return 0;
}

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
	int n;
	switch(cmd->code) {
		case PSSCLI_CMD_BASEADDR:
			strcpy(in_buf + LWS_PRE, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"pss_baseAddr\",\"params\":null}");
			n = lws_write(psscli_ws.w, &in_buf[LWS_PRE], strlen(&in_buf[LWS_PRE]), LWS_WRITE_TEXT);
			break;

	}
	return 0;
}
