#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#include "server.h"
#include "ws.h"
#include "cmd.h"
#include "error.h"
#include "sync.h"
#include "config.h"

// control
extern struct psscli_config conf;
static pid_t run;
pthread_cond_t pt_cond_main;
pthread_mutex_t pt_lock_main;
int error;

pthread_t pt_server;
pthread_t pt_ws;

pthread_t pt_client[8];
int clientcount;

int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

struct threadspec {
	int n;
	char e[1024];
};

void croak(int s) {
	fprintf(stderr, "caught signal %d\n", s);
	pthread_cond_signal(&pt_cond_main);
}

int try_timeout(int (*function)(), int timeout_seconds) {
	time_t start;
	time_t now;
	struct timespec ts;

	start = time(NULL);

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	while (!function()) {
		now = time(NULL);
		if (now - start >= timeout_seconds) {
			return 1;
		}
		nanosleep(&ts, NULL);
	}

	return 0;
}

void *ws(void *args) {
	psscli_ws_start();
	pthread_exit(NULL);
	return NULL;
}

void *server(void *args) {
	psscli_server_start();
	pthread_exit(NULL);
	return NULL;
}

void *client(void *arg) {
	int c;
	char b[1024];
	struct threadspec *ths;

	struct sockaddr_un rs;
	int sd;

	fprintf(stderr, "starting client %d\n", c);

	ths = (struct threadspec*)arg;	

	// set up client socket
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		pthread_exit(NULL);
		error = PSSCLI_ESOCK;
		return NULL;
	}
	rs.sun_family = AF_UNIX;
	strcpy(rs.sun_path, conf.sock);
	c = strlen(rs.sun_path) + sizeof(rs.sun_family);

	b[0] = PSSCLI_CMD_BASEADDR;
	if (connect(sd, (struct sockaddr *)&rs, c) == -1) {
		error = PSSCLI_ESOCK;
		strcpy(ths->e, "connect fail");
		pthread_exit((void*)ths);
		return NULL;
	} else if (send(sd, b, 4, 0) == -1) {
		error = PSSCLI_ESOCK;
		strcpy(ths->e, "send fail");
		pthread_exit((void*)ths);
		return NULL;
	} else if ((c = recv(sd, b, 1024, 0)) == -1) {
		strcpy(ths->e, "receive fail");
		pthread_exit((void*)ths);
		return NULL;
	} else {
		fprintf(stderr, "client got: %s\n", b);
	}
	psscli_stop();
	close(sd);
	pthread_exit(NULL);
	return NULL;
}

int main(int argc, char **argv) {
	int c;
	char b[1024];
	char e[1024];
	struct timespec ts;
	struct threadspec ths;

	// init
	run = getpid();
	struct sigaction sa;
	ths.n = 0;
	ths.e[0] = 0;

	// we use sigint to end
	sa.sa_handler = croak;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	// configure
	psscli_start();
	psscli_config_init();
	if (psscli_config_parse(argc, argv, 1024, b)) {
		fprintf(stderr, "config error: %s", b);
		return 1;
	}
	
	fprintf(stderr, "testing with %s:%d on %s\n", conf.host, conf.port, conf.sock);

	// start the message queue
	if (psscli_queue_start(16, 16)) {
		fprintf(stderr, "websockets relay creation fail\n");
		return 2;
	}

	// start the websockets relay
	if (psscli_ws_init(ws_callback, "2.0")) {
		fprintf(stderr, "websockets relay creation fail\n");
		return 3;
	}
	if (pthread_create(&pt_ws, NULL, ws, NULL) == -1) {
		fprintf(stderr, "server creation fail\n");
		return 4;
	}
	if (try_timeout(psscli_ws_ready, 3)) {
		fprintf(stderr, "websockets relay start fail\n");
		return 5;
	}

	// start the socket server
	if (pthread_create(&pt_server, NULL, server, NULL) == -1) {
		fprintf(stderr, "server creation fail\n");
		return 6;
	}
	if (try_timeout(psscli_server_status, 3)) {
		fprintf(stderr, "server start fail\n");
		return 7;
	}

	c = clientcount;
	if (pthread_create(&pt_client[0], NULL, client, (void*)&ths)) {
		fprintf(stderr, "server start fail\n");
		return 8;
	}

	pthread_join(pt_client[0], NULL);

	psscli_ws_stop();
	psscli_server_stop();
	psscli_queue_stop();

	pthread_join(pt_server, NULL);
	pthread_join(pt_ws, NULL);

	return 0;
}
