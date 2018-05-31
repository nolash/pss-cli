#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "ws.h"
#include "cmd.h"
#include "sync.h"

pthread_t pt_main;
extern pthread_cond_t pt_cond_write;
extern pthread_mutex_t pt_lock_state;
extern pthread_mutex_t pt_lock_queue;
extern psscli_cmd psscli_cmd_current;

//
// protocols for testing
//

// disconnects when connect is made
int disconnect_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int f;
	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			fprintf(stderr, "connected, so disconnecting\n");
			psscli_ws.pid = 0;
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			psscli_ws.pid = 0;
			break;
	}
	return 0;
}

// send messages when receiving
// after sent, signal writeloop again
// when third message is received, trigger main loop exit
int send_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int f;
	static int counter;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			pthread_mutex_lock(&pt_lock_state);
			fprintf(stderr, "got data %d %p\n", psscli_cmd_current.id, user);
			if (psscli_cmd_current.id == 44) {
				psscli_ws.pid = 0;
			} else {
				pthread_cond_signal(&pt_cond_write);
			}
			pthread_mutex_unlock(&pt_lock_state);
			break;
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			psscli_ws.connected = 1;
			fprintf(stderr, "connected\n");
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			fprintf(stderr, "connection error\n");
			psscli_ws.pid = 0;
			break;
	}
	return 0;

}

//
// test code
//

// test 1
int test_disconnect() {
	if (psscli_ws_init(disconnect_proto, "2.0")) {
		return 1;
	}
	psscli_ws_connect();
	psscli_ws_free();
}

// test 2
void *connect_thread(void *args) {
	psscli_ws_connect();
	pthread_exit(&pt_main);
	return NULL;
}

// \todo cleanup threads on exit
int test_send() {
	int i;
	int j;
	int r;
	struct timespec ts;
	psscli_cmd cmd[3];

	psscli_queue_start(3, 3);
	if (psscli_ws_init(send_proto, "2.0")) {
		return 1;
	}
	if (r = pthread_create(&pt_main, NULL, connect_thread, NULL)) {
		return 2;
	}

	// create three messages and add them to the queue
	for (i = 0; i < 3; i++) {
		psscli_cmd_alloc(&cmd[i], 0);
		cmd[i].id = 42+i;
		pthread_mutex_lock(&pt_lock_queue);
		if (psscli_cmd_queue_add(&cmd[i])) {
			pthread_mutex_unlock(&pt_lock_queue);
			for (j = i; j >= 0; j--) {
				psscli_cmd_free(&cmd[j]);
			} 			psscli_ws.pid = 0;
			fprintf(stderr, "add cmd fail on index %d\n", i);
			return 3;
		} 
		pthread_mutex_unlock(&pt_lock_queue);
	}
	pthread_mutex_unlock(&pt_lock_queue);

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	// wait until websocket is connected
	while (!psscli_ws.connected) {
		nanosleep(&ts, NULL);
	}

	// signal writeloop to process messages
	fprintf(stderr, "signalling writeloop\n");
	pthread_cond_signal(&pt_cond_write);
	
	pthread_join(pt_main, NULL);
	psscli_queue_stop();
	psscli_ws_free();
}

int main() {
	psscli_ws.host = "192.168.0.42";
	psscli_ws.origin = psscli_ws.host;
	psscli_ws.port = 8546;
	psscli_ws.ssl = 0;

	if (test_disconnect()) {
		return 1;	
	}
	if (test_send()) {
		return 2;
	}

	return 0;
}
