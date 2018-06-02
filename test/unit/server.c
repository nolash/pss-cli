#include <sys/un.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

pthread_t pt_server;

#include "server2.h"
#include "ws.h"
#include "config.h"
#include "cmd.h"

void *server(void *arg) {
	psscli_server_start();
	pthread_exit(NULL);
	return NULL;
}

int test_sock() {
	struct timespec ts;
	struct sockaddr_un rs;
	int sd;
	int l;
	unsigned char b[1024];

	psscli_config_init();
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
	l = strlen(rs.sun_path) + sizeof(rs.sun_family);
	

	b[0] = PSSCLI_CMD_BASEADDR;
	strcpy(b+1, "foo");
	if (connect(sd, (struct sockaddr *)&rs, l) == -1) {
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 3;
	} else if (send(sd, b, 4, 0) == -1) {
		fprintf(stderr, "send 1 fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 3;
	} else if (l = recv(sd, &b, 1024, 0) <= 0) {
		fprintf(stderr, "recv 1 fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 3;
	}
	fprintf(stderr, "recv 1: %d\n", *((int*)b));

	b[0] = PSSCLI_CMD_BASEADDR;
	strcpy(b+1, "bar");
	if (send(sd, b, 4, 0) == -1) {
		fprintf(stderr, "send 2 fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 4;
	} else if (l = recv(sd, &b, 1024, 0) <= 0) {
		fprintf(stderr, "recv 2 fail\n");
		psscli_server_stop();
		pthread_join(pt_server, NULL);
		return 4;
	}
	fprintf(stderr, "recv 2: %d\n", *((int*)b));

	close(sd);
	psscli_server_stop();
	pthread_join(pt_server, NULL);
	psscli_queue_stop();

	return 0;
}

int main() {
	if (test_sock()) {
		return 1;
	}
	return 0;
}
