#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include "config.h"
#include "ws.h"
#include "server.h"
#include "std.h"
#include "error.h"

extern struct psscli_ws_ psscli_ws;
extern struct psscli_config conf;

pthread_t p1;
pthread_t p2;

// sigint handler
void croak(int sig) {
	fprintf(stderr, "killing %d by thread %d\n", psscli_ws.pid, pthread_self());
	psscli_ws.pid = 0;
}

// wrappers for pthread routines
void* server_start(void *v) {
	fprintf(stderr, "sockserver thread: %d\n", p1);
	psscli_server_start(NULL);
	pthread_exit(NULL);	
}

void* ws_connect(void *v) {
	fprintf(stderr, "websocket thread: %d\n", p2);
	psscli_ws_connect(NULL);
	pthread_kill(p1, SIGINT);
	pthread_exit(NULL);
}

int main(int argc, char **argv) {
	int r;
	short sl;
	char buf[1024];

	struct sigaction sa;
	char c[1024];
	struct timespec ts;

	if (psscli_config_init()) {
		exit(PSSCLI_ENOINIT);
	}
	if (psscli_config_parse(argc, argv, 1024, buf)) {
		exit(PSSCLI_ENOINIT);
	}

	// initialize websocket params
	psscli_ws.host = conf.host;
	psscli_ws.origin = conf.host;
	psscli_ws.port = conf.port;
	psscli_ws.ssl = 0;
	if (psscli_ws_init(psscli_server_cb, "2.0")) {
		return 1;
	}

	// separate threads for the unix socket server (for piping commands) and websocket
	r = pthread_create(&p1, NULL, server_start, NULL);
	if (r) {
		return 2;
	}
	r = pthread_create(&p2, NULL, ws_connect, NULL);
	if (r) {
		return 2;
	}

	// we use sigint to end
	sa.sa_handler = croak;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	// wait until websocket is connected
	while (!psscli_ws.connected) {
		nanosleep(&ts, NULL);
	}

	fprintf(stderr, "connected\n");

	while (psscli_ws.pid) {
		nanosleep(&ts, NULL);
	}

	fprintf(stderr, "exiting\n");

	raise(SIGINT);
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
	psscli_ws_free();

	return 0;
}
