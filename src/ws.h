#ifndef PSSCLI_WS_H_
#define PSSCLI_WS_H_

#define PSSCLI_WS_DEFAULT_HOST "127.0.0.1"
#define PSSCLI_WS_DEFAULT_PORT 8546
#define PSSCLI_WS_PROTOCOL_NAME "pss"
#define PSSCLI_WS_RX_BUFFER 128
#define PSSCLI_WS_TX_BUFFER 1024
#define PSSCLI_WS_LOOP_TIMEOUT 100

#include <libwebsockets.h>
#include <pthread.h>
#include "cmd.h"

struct psscli_ws_ {
	pid_t pid; // maintain event loop while this is >0
	int connected; // set when websocket is connected
	int notify[2]; // socket fds for triggering command sends
	const char* host; // websocket host to connect to
	const char* origin; // websocket CORS origin string
	int port; // websocket port to connect to
	int ssl; // websocket ssl yes/no (not implemented)
	char buf[PSSCLI_WS_RX_BUFFER + LWS_PRE]; // websocket receive buffer
	int id; // websocket RPC sequence id (increments for every successful RPC call in same session)

	// below is libwebsocket stuff
	struct lws *w; 
	struct lws_client_connect_info wi;
	struct lws_context_creation_info wci;
	struct lws_context *ctx;
} psscli_ws;

typedef int(*psscli_ws_callback)(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

int psscli_ws_init(psscli_ws_callback callback, const char *version);
int psscli_ws_connect();
void psscli_ws_free();
int psscli_ws_send(psscli_cmd *cmd);

#endif // PSSCLI_WS_H_

