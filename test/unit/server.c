#include <sys/un.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include "server.h"
#include "ws.h"
#include "config.h"
#include "cmd.h"
#include "sync.h"

pthread_t pt_server;

extern pthread_cond_t pt_cond_reply;

void *server(void *arg) {
	psscli_server_start();
	pthread_exit(NULL);
	return NULL;
}

void *process_reply(void *arg);

// \todo find a nicer way to sync test receive on socket with the parse reply loop
int test_reply() {
	int c;
	int i;
	struct timespec ts;
	unsigned char b[1024];

	// socket
	struct sockaddr_un rs;
	int sd;

	// test socket control 
	int rsd;
	int *rsdptr;
	int fails;

	psscli_response *response;

	psscli_config_init();
	psscli_start();
	psscli_queue_start(100, 100);
	if ((pthread_create(&pt_server, NULL, server, NULL)) == -1) {
		return 1;
	}

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 5000;

	// wait until server is up
	while (!psscli_server_status() == PSSCLI_SERVER_STATUS_RUNNING) {
		nanosleep(&ts, NULL);
	}


	// setup socket
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 2;
	}
	rs.sun_family = AF_UNIX;
	strcpy(rs.sun_path, conf.sock);
	c = strlen(rs.sun_path) + sizeof(rs.sun_family);
	
	if (connect(sd, (struct sockaddr *)&rs, c) == -1) {
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 3;
	}

	// get the socket descriptor
	// (requires TESTING macro defined)
	if (recv(sd, b, sizeof(int*)+sizeof(int), 0) == -1) {
		fprintf(stderr, "recv sd fail\n", i);
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 4;
	}

	// recoved the pointers used
	memcpy(&rsdptr, b, sizeof(int*));
	memcpy(&rsd, b+sizeof(int*), sizeof(int));

	// send messages
	for (i = 0; i < 100; i++) {
		response = malloc(sizeof(psscli_response));
		response->id = i;
		response->status = PSSCLI_RESPONSE_STATUS_PARSED;
		response->length = 3;
		response->sd = rsd;
		response->sdptr = rsdptr;
		strcpy(response->content, "foo");

		if (psscli_response_queue_add(response) == -1) {
			psscli_server_stop();	
			pthread_join(pt_server, NULL);
			return 4;
		}
	}

	fails = 0;
	for (i = 0; i < 100; i++) {
		pthread_cond_broadcast(&pt_cond_reply);

		if ((c = recv(sd, b, 3, MSG_DONTWAIT)) == -1) {
			fails++;
			if (fails > 1000) {
				fprintf(stderr, "recv %d fail\n", i);
				psscli_server_stop();
				pthread_join(pt_server, NULL);
				return 5;
			}
			i--;
			nanosleep(&ts, NULL);
			continue;
		}
		b[c] = 0;

		fprintf(stderr, "reply %d got: %s\n", i, b);
	}

	psscli_server_stop();
	pthread_join(pt_server, NULL);
	return 0;
}

// \todo segfaults on shutdown
// \todo move write reply to separate thread triggered by pthread cond
int test_sock() {
	struct timespec ts;
	struct sockaddr_un rs;
	int sd;
	int c;

	int rsd;
	int *rsdptr;

	unsigned char b[1024];
	psscli_cmd *cmd;
	psscli_response *response;

	psscli_config_init();

	psscli_start();
	psscli_queue_start(2, 2);

	if (pthread_create(&pt_server, NULL, server, NULL) == -1) {
		return 1;
	}

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	// wait until websocket is connected
	while (!psscli_server_status() == PSSCLI_SERVER_STATUS_RUNNING) {
		nanosleep(&ts, NULL);
	}

	// send command on socket
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 2;
	}
	rs.sun_family = AF_UNIX;
	strcpy(rs.sun_path, conf.sock);
	c = strlen(rs.sun_path) + sizeof(rs.sun_family);
	

	if (connect(sd, (struct sockaddr *)&rs, c) == -1) {
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 3;
	}

	// get the socket descriptor
	// (requires TESTING macro defined)
	if (recv(sd, b, sizeof(int*)+sizeof(int), 0) == -1) {
		fprintf(stderr, "recv sd fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 4;
	}

	// recover the pointers used
	memcpy(&rsdptr, b, sizeof(int*));
	memcpy(&rsd, b+sizeof(int*), sizeof(int));

	b[0] = PSSCLI_CMD_BASEADDR;
	strcpy(b+1, "foo");
	if (send(sd, b, 4, 0) == -1) {
		fprintf(stderr, "send 1 fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 3;
	}

	while (psscli_cmd_queue_peek() == NULL) {
		nanosleep(&ts, NULL);
	}

	cmd = psscli_cmd_queue_next();
	if (cmd == NULL) {
		fprintf(stderr, "can't get cmd\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 4;
	}

	response = malloc(sizeof(psscli_response));
	response->id = cmd->id;
	response->status = PSSCLI_RESPONSE_STATUS_PARSED;
	response->sd = rsd;
	response->sdptr = rsdptr;
	strcpy(response->content, "baz");
	response->length = 3;
	if (psscli_response_queue_add(response) == -1) {
		fprintf(stderr, "can't add response\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 5;
	}

	pthread_cond_signal(&pt_cond_reply);
	if ((c = recv(sd, b, 1024, 0)) == -1) {
		fprintf(stderr, "can't receive response\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 6;
	}

	b[c] = 0;
	fprintf(stderr, "recv 1: %s\n", b);
	psscli_cmd_free(cmd);

	b[0] = PSSCLI_CMD_BASEADDR;
	strcpy(b+1, "bar");
	if (send(sd, b, 4, 0) == -1) {
		fprintf(stderr, "send 2 fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 7;
	}

	while (psscli_cmd_queue_peek() == NULL) {
		nanosleep(&ts, NULL);
	}

	cmd = psscli_cmd_queue_next();
	if (cmd == NULL) {
		fprintf(stderr, "can't get cmd\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 8;
	}

	response = malloc(sizeof(psscli_response));
	response->id = cmd->id;
	response->status = PSSCLI_RESPONSE_STATUS_PARSED;
	response->sd = rsd;
	response->sdptr = rsdptr;
	strcpy(response->content, "xyzzy");
	response->length = 5;
	if (psscli_response_queue_add(response) == -1) {
		fprintf(stderr, "can't add response\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 9;
	}

	pthread_cond_signal(&pt_cond_reply);
	if ((c = recv(sd, b, 1024, 0)) == -1) {
		fprintf(stderr, "can't receive response\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 10;
	}

	b[c] = 0;
	fprintf(stderr, "recv 2: %s\n", b);
	psscli_cmd_free(cmd);

	close(sd);
	psscli_stop();
	psscli_server_stop();
	pthread_join(pt_server, NULL);
	psscli_queue_stop();

	return 0;
}

int main() {
	if (test_reply()) {
		return 1;
	}
	if (test_sock()) {
		return 2;
	}
	return 0;
}
