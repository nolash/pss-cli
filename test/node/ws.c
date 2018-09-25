#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "ws.h"
#include "cmd.h"
#include "sync.h"
#include "config.h"

pthread_t pt_main;
extern pthread_cond_t pt_cond_write;
extern pthread_mutex_t pt_lock_state;
extern pthread_mutex_t pt_lock_queue;
//extern psscli_cmd psscli_cmd_current;
psscli_cmd psscli_cmd_current_out;
psscli_cmd psscli_cmd_current_in;

int json_parse(psscli_cmd *cmd);

int result_count;

// connection
void *connect_thread(void *args) {
	if (psscli_ws_start()) {
		fprintf(stderr, "ws connect error\n");
	}
	pthread_exit(&pt_main);
	return NULL;
}

//
// protocols for testing
//

// disconnects when connect is made
int disconnect_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int f;
	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			fprintf(stderr, "connected, so disconnecting\n");
			psscli_stop();
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			psscli_ws.pid = 0;
			break;
	}
	return 0;
}

// send messages on writeloop signal
// after sent, signal writeloop again
// when third message is received, trigger main loop exit
int send_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int r;
	static int counter;
	psscli_cmd *lcmd;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			pthread_mutex_lock(&pt_lock_queue);
			lcmd = psscli_cmd_queue_next(PSSCLI_QUEUE_OUT);
			pthread_mutex_unlock(&pt_lock_queue);
			fprintf(stderr, "got data %d %p\n", lcmd->id, user);
			r = psscli_ws_send(lcmd);
			if (r) {
				pthread_mutex_lock(&pt_lock_queue);
				psscli_cmd_queue_add(PSSCLI_QUEUE_OUT, lcmd); // put it back into the queue
				pthread_mutex_unlock(&pt_lock_queue);
				fprintf(stderr, "send problem: %d\n", r);
			}
			lcmd->status |= PSSCLI_STATUS_X;
			lcmd->code = 0;
			pthread_mutex_lock(&pt_lock_queue);
			psscli_cmd_queue_add(PSSCLI_QUEUE_X, lcmd);
			pthread_mutex_unlock(&pt_lock_queue);
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
			psscli_ws.pid = 0;
			pthread_mutex_unlock(&pt_lock_state);
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			fprintf(stderr, "recv packet: %s\n", in);
			pthread_cond_signal(&pt_cond_write);
			if (++result_count == 3) {
				psscli_stop();
			}
			break;
	}
	return 0;

}

// send message and receive response
// after sent, signal writeloop again
// when third message is received, trigger main loop exit
int recv_proto(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int r;
	char *t;
	psscli_cmd *lcmd;
	psscli_cmd *rcmd;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			pthread_mutex_lock(&pt_lock_queue);
			lcmd = psscli_cmd_queue_next(PSSCLI_QUEUE_OUT);
			pthread_mutex_unlock(&pt_lock_queue);
			fprintf(stderr, "got data %d %p\n", lcmd->id, user);
			r = psscli_ws_send(lcmd);
			if (r) {
				pthread_mutex_lock(&pt_lock_queue);
				psscli_cmd_queue_add(PSSCLI_QUEUE_OUT, lcmd); // put it back into the queue
				pthread_mutex_unlock(&pt_lock_queue);
				fprintf(stderr, "send problem: %d\n", r);
			}
			lcmd->status |= PSSCLI_STATUS_X;
			lcmd->code = 0;
			pthread_mutex_lock(&pt_lock_queue);
			psscli_cmd_queue_add(PSSCLI_QUEUE_X, lcmd);
			pthread_mutex_unlock(&pt_lock_queue);
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
			psscli_ws.pid = 0;
			pthread_mutex_unlock(&pt_lock_state);
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			fprintf(stderr, "part recv: %s\n", in);
			pthread_mutex_lock(&pt_lock_queue);
			lcmd = psscli_cmd_queue_peek(PSSCLI_QUEUE_X);	
			pthread_mutex_unlock(&pt_lock_queue);
			strcpy(lcmd->src+lcmd->srclength, in);
			lcmd->srclength += len;

			// rpc json terminates at newline, if we have it we can dispatch to handler 
			t = in;
			while (1) {
				t = strchr(t, 0x7b);
				if (t == 0x0) {
					break;
				}
				t++;
				lcmd->code++;
			}
			t = in;
			while (1) {
				t = strchr(t, 0x7d);
				if (t == 0x0) {
					break;
				}
				t++;
				lcmd->code--;
			}
			if (!lcmd->code) {
				fprintf(stderr, "received response: %s\n", lcmd->src);
				lcmd->status |= PSSCLI_STATUS_COMPLETE;
				pthread_mutex_lock(&pt_lock_queue);
				lcmd = psscli_cmd_queue_next(PSSCLI_QUEUE_X);
				if (psscli_cmd_queue_add(PSSCLI_QUEUE_IN, lcmd) == -1) {
					fprintf(stderr, "failed to add to queue\n");
					return 1;
				}
				pthread_mutex_unlock(&pt_lock_queue);
			}
			break;

	}
	return 0;

}

//
// test code
//

// test 1
int test_disconnect() {
	struct timespec ts;
	int r;

	psscli_start();
	if (psscli_ws_init(disconnect_proto, "2.0")) {
		return 1;
	}
	if ((r = pthread_create(&pt_main, NULL, connect_thread, NULL))) {
		return 2;
	}

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 500000000L;

	// wait until websocket is connected
	while (psscli_running()) {
		nanosleep(&ts, NULL);
	}

	psscli_stop();
	psscli_ws_stop();
	psscli_queue_stop();

	pthread_join(pt_main, NULL);

	return 0;
}

// test 2


// \todo cleanup threads on exit (currently hangs)
int test_send() {
	int i;
	int j;
	int r;
	struct timespec ts;
	psscli_cmd *cmd[3];

	result_count = 0;

	psscli_start();
	psscli_queue_start(3);
	if (psscli_ws_init(send_proto, "2.0")) {
		return 1;
	}
	if (r = pthread_create(&pt_main, NULL, connect_thread, NULL)) {
		return 2;
	}

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 500000000L;

	// create three messages and add them to the queue
	for (i = 0; i < 3; i++) {
		psscli_cmd_alloc(&cmd[i], 0);
		cmd[i]->code = PSSCLI_CMD_BASEADDR;
		cmd[i]->id = 42+i;
		pthread_mutex_lock(&pt_lock_queue);
		if (psscli_cmd_queue_add(PSSCLI_QUEUE_OUT, cmd[i])) {
			pthread_mutex_unlock(&pt_lock_queue);
			for (j = i; j >= 0; j--) {
				psscli_cmd_free(cmd[j]);
			} 
			psscli_ws.pid = 0;
			fprintf(stderr, "add cmd fail on index %d\n", i);
			return 3;
		} 
		pthread_mutex_unlock(&pt_lock_queue);
	}

	pthread_mutex_unlock(&pt_lock_queue);
	// wait until websocket is connected
	while (!psscli_ws_ready()) {
		fprintf(stderr, "wait for connect\n");
		nanosleep(&ts, NULL);
	}

	// signal writeloop to process messages
	fprintf(stderr, "signalling writeloop\n");
	pthread_cond_signal(&pt_cond_write);
	
	while (psscli_running()) {
		nanosleep(&ts, NULL);
	}

	psscli_stop();
	psscli_ws_stop();
	psscli_queue_stop();
	
	pthread_join(pt_main, NULL);

	return 0;
}

// test 3


// \todo cleanup threads on exit
int test_recv() {
	int i;
	int j;
	int r;
	struct timespec ts;
	psscli_cmd *cmd;
	psscli_cmd *cmd_resp;

	psscli_start();
	psscli_queue_start(3);
	if (psscli_ws_init(recv_proto, "2.0")) {
		return 1;
	}
	if (r = pthread_create(&pt_main, NULL, connect_thread, NULL)) {
		return 2;
	}

	psscli_cmd_alloc(&cmd, 0);
	cmd->code = PSSCLI_CMD_BASEADDR;
	cmd->id = 1;
	pthread_mutex_lock(&pt_lock_queue);
	if (psscli_cmd_queue_add(PSSCLI_QUEUE_OUT, cmd)) {
		pthread_mutex_unlock(&pt_lock_queue);
		for (j = i; j >= 0; j--) {
			psscli_cmd_free(cmd);
		}
		psscli_ws.pid = 0;
		fprintf(stderr, "add cmd fail on index %d\n", i);
		return 3;
	} 
	pthread_mutex_unlock(&pt_lock_queue);

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	// wait until websocket is connected
	while (!psscli_ws_ready()) {
		nanosleep(&ts, NULL);
	}

	// signal writeloop to process messages
	fprintf(stderr, "signalling writeloop\n");
	pthread_cond_signal(&pt_cond_write);
	
	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 500000000L;

	// check for the response
	while (1) {
		nanosleep(&ts, NULL);
		fprintf(stderr, "waiting for message received\n");
		cmd_resp = psscli_cmd_queue_next(PSSCLI_QUEUE_IN);
		if (cmd_resp == NULL) {
			continue;
		}
		if (!(cmd_resp->status & PSSCLI_STATUS_COMPLETE)) {
			fprintf(stderr, "wrong status: %d\n", cmd_resp->status);
			free(cmd_resp);
			break;	
		}
		if (json_parse(cmd_resp)) {
			fprintf(stderr, "parse response error!");
		}
		fprintf(stderr, "got message: %s\n", *(cmd_resp->values));
		//free(cmd_resp);
		break;
	}
	
	psscli_stop();
	psscli_ws_stop();
	psscli_queue_stop();

	pthread_join(pt_main, NULL);
	psscli_queue_stop();
	psscli_ws_stop();
}

// \todo segfaults on last test when all tests are run 
int main(int argc, char **argv) {

	psscli_config_init();
	psscli_config_parse(argc, argv, 0, NULL);

	if (test_disconnect()) {
		return 1;	
	}
	if (test_send()) {
		return 2;
	}
	if (test_recv()) {
		return 3;
	}

	return 0;
}
