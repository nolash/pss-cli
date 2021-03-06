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

/***
 * \short connects to websocket on swarm (pss) node
 * \description polls for servicing the websocket and reads from command unix socket pipe every turn
 * \todo retry loop on connect fail
 */ 
int psscli_ws_start();
void psscli_ws_stop();
int psscli_ws_ready();

/***
 * \short send command on websocket
 * \description converts command structure to json rpc and sends to websocket. Called from callback when websocket is writable
 * \todo fail handling
 */
int psscli_ws_send(psscli_cmd *cmd);

#endif // PSSCLI_WS_H_

