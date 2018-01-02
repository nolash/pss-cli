#include "ws.h"

int callback_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int f;
	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			psscli_ws.run = 0;
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			psscli_ws.run = 0;
			break;
	}
	return 0;
}

int main() {
	psscli_ws.host = "localhost";
	psscli_ws.origin = psscli_ws.host;
	psscli_ws.port = 8546;
	psscli_ws.ssl = 0;
	if (psscli_ws_init(callback_proto)) {
		return 1;
	}
	psscli_ws_connect();
	psscli_ws_free();
}
