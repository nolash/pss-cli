#ifndef PSSCLI_WS_H_
#define PSSCLI_WS_H_

#define PSSCLI_WS_PROTOCOL_NAME "pss"
#define PSSCLI_WS_RX_BUFFER 128
#define PSSCLI_WS_LOOP_TIMEOUT 250

#include <libwebsockets.h>

enum psscli_cmd_code {
	PSSCLI_CMD_NONE,
	PSSCLI_CMD_BASEADDR,
	PSSCLI_CMD_SETPEERPUBLICKEY
};

typedef struct psscli_cmd {
	enum psscli_cmd_code code;
	char **values;
	unsigned char valuecount;
} psscli_cmd;


struct psscli_ws_ {
	pid_t pid;
	void *trigger;
	struct lws *w;
	struct lws_client_connect_info wi;
	struct lws_context_creation_info wci;
	const char* host;
	const char* origin;
	int port;
	int ssl;
	struct lws_context *ctx;
	char buf[PSSCLI_WS_RX_BUFFER + LWS_PRE];
} psscli_ws;

typedef int(*psscli_ws_callback)(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

int psscli_ws_init(psscli_ws_callback callback);
int psscli_ws_connect();
void psscli_ws_free();
int psscli_ws_send(psscli_cmd *cmd);

#endif // PSSCLI_WS_H_

