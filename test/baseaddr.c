#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <json-c/json.h>

static volatile int force_exit;
int in_buf_crsr;
char in_buf[2048 + LWS_PRE];
int in_depth = 0;
int sent = 0;

int callback_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int n;
	char s[2048];
	char *t;
	json_tokener *jt;
	json_object *jo;
	json_object *jv; 
	json_bool jr;

	switch (reason) {
		case LWS_CALLBACK_WSI_CREATE:
			sprintf(s, "create: %p\n", wsi);
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			force_exit = 1;
			sprintf(s, "connerr: %s (%d)\n", (char*)in), len;
			break;
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lws_callback_on_writable(wsi);
			break;
		case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
			sprintf(s, "pre: %p\n", wsi);
			break;
		case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
			sprintf(s, "completed!");
			force_exit = 1;
			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			sprintf(s, "writeable\n");
			if (sent) {
				break;
			}
			in_buf_crsr = 0;
			strcpy(in_buf + LWS_PRE, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"pss_baseAddr\",\"params\":null}");
			n = lws_write(wsi, &in_buf[LWS_PRE], strlen(&in_buf[LWS_PRE]), LWS_WRITE_TEXT);
			sent = 1;
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			strcpy(in_buf + in_buf_crsr, in);
			in_buf_crsr += len;
			sprintf(s, "got data (%d): %s\n", strlen(in), in);
			t = in;
			while (1) {
				t = strchr(t, 0x7b);
				if (t == 0x0) {
					break;
				}
				t++;
				in_depth++;
			}
			t = in;
			while (1) {
				t = strchr(t, 0x7d);
				if (t == 0x0) {
					break;
				}
				t++;
				in_depth--;
			}
			if (!in_depth) {
				*(in_buf + in_buf_crsr) = 0;
				jt = json_tokener_new();
				jo = json_tokener_parse_ex(jt, in_buf, in_buf_crsr);
				jr = json_object_object_get_ex(jo, "result", &jv);
				sprintf(s, "result (%d): %s\n", (int)jr, json_object_get_string(jv));
				json_tokener_free(jt);
				force_exit = 1;
			}
			break;
		default:
			sprintf(s, "cb: %d\n", reason);
	}

	lwsl_notice(s);
	return 0;
}

int main() {
	struct lws_client_connect_info wi;
	struct lws *w;
	struct lws_context_creation_info wci;
	struct lws_context *context;
	struct lws_protocols protocols[] = {
		{ "pss", callback_proto, 0, 20 },
		{ NULL, NULL, 0, 0}
	};
	struct lws_extension exts[] = {
		{ NULL, NULL, NULL }
	};

	force_exit = 0;
	memset(&wci, 0, sizeof(wci));
	memset(&wi, 0, sizeof(wi));
	wci.uid = -1;
	wci.gid = -1;
	wci.port = CONTEXT_PORT_NO_LISTEN;
	wci.protocols = protocols;
	wci.extensions = exts;

	context = lws_create_context(&wci);
	if (context == NULL) {
		lwsl_err("cant create ws context");
		return 1;
	}
	wi.context = context;
	wi.address = "192.168.0.42";
	wi.port = 8546;
	wi.host = "192.168.0.42";
	wi.origin = "localhost";
	wi.protocol = protocols[0].name;
	wi.ssl_connection = 0;
	wi.path = "/";
	wi.pwsi = &w;
	wi.ietf_version_or_minus_one = -1;

	lws_client_connect_via_info(&wi);
	while (!force_exit) {
		lws_service(context, 500);
	}

	lws_context_destroy(context);
	return 0;
}
