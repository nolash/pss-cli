#include <stdio.h>
#include <pthread.h>

#include "ws.h"
#include "cmd.h"
#include "sync.h"

extern pthread_cond_t pt_cond_parse;
extern pthread_mutex_t pt_lock_state;
extern pthread_mutex_t pt_lock_queue;

// send message and receive response
// after sent, signal writeloop again
// when third message is received, trigger main loop exit
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int r;
	char *t;
	psscli_cmd lcmd;
	psscli_response *response_result;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			pthread_mutex_lock(&pt_lock_state);
			fprintf(stderr, "got data %d %p\n", psscli_cmd_current.id, user);
			psscli_cmd_alloc(&lcmd, psscli_cmd_current.valuecount);
			psscli_cmd_copy(&lcmd, &psscli_cmd_current);
			pthread_mutex_unlock(&pt_lock_state);
			r = psscli_ws_send(&lcmd);
			if (r) {
				fprintf(stderr, "send problem: %d\n", r);
			}
			break;
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			pthread_mutex_lock(&pt_lock_state);
			psscli_ws.connected = 1;
			pthread_mutex_unlock(&pt_lock_state);
			fprintf(stderr, "connected\n");
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			fprintf(stderr, "connection error\n");
			pthread_mutex_lock(&pt_lock_state);
			psscli_stop();
			pthread_mutex_unlock(&pt_lock_state);
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			fprintf(stderr, "part recv: %s\n", in);
			pthread_mutex_lock(&pt_lock_state);
			strcpy(psscli_response_current.content+psscli_response_current.length, in);
			psscli_response_current.length += len;

			// rpc json terminates at newline, if we have it we can dispatch to handler 
			t = in;
			while (1) {
				t = strchr(t, 0x7b);
				if (t == 0x0) {
					break;
				}
				t++;
				psscli_response_current.id++;
			}
			t = in;
			while (1) {
				t = strchr(t, 0x7d);
				if (t == 0x0) {
					break;
				}
				t++;
				psscli_response_current.id--;
			}
			pthread_mutex_unlock(&pt_lock_state);
			if (!psscli_response_current.id) {
				response_result = malloc(sizeof(psscli_response));
				memcpy(response_result, &psscli_response_current, sizeof(psscli_response));
				fprintf(stderr, "received response: %s\n", response_result->content);
				response_result->status = PSSCLI_RESPONSE_STATUS_RECEIVED;
				pthread_mutex_lock(&pt_lock_queue);
				if (psscli_response_queue_add(response_result) == -1) {
					pthread_mutex_unlock(&pt_lock_queue);
					fprintf(stderr, "failed to add to queue\n");
					return 1;
				}
				pthread_mutex_unlock(&pt_lock_queue);
				psscli_response_current.status = PSSCLI_RESPONSE_STATUS_NONE;
				pthread_cond_signal(&pt_cond_parse);
			}
			break;

	}
	return 0;

}
