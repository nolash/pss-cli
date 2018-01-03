#include <sys/wait.h>
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include "server.h"
#include "ws.h"

extern struct psscli_ws_ psscli_ws;
pthread_t p1;
pthread_t p2;

void croak(int sig) {
	printf("killing %d by thread %d\n", psscli_ws.pid, pthread_self());
	psscli_ws.pid = 0;
}

void* server_start(void *v) {
	printf("sockserver thread: %d\n", p1);
	psscli_server_start(NULL);
	pthread_exit(NULL);	
}

void* ws_connect(void *v) {
	printf("websocket thread: %d\n", p2);
	psscli_ws_connect(NULL);
	pthread_kill(p1, SIGINT);
	pthread_exit(NULL);
}

int main() {
	int r;
	struct sigaction sa;

	psscli_ws.host = "localhost";
	psscli_ws.origin = psscli_ws.host;
	psscli_ws.port = 8546;
	psscli_ws.ssl = 0;
	if (psscli_ws_init(psscli_server_cb)) {
		return 1;
	}

	r = pthread_create(&p1, NULL, server_start, NULL);
	if (r) {
		return 2;
	}
	r = pthread_create(&p2, NULL, ws_connect, NULL);
	if (r) {
		return 2;
	}

	sa.sa_handler = croak;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	pthread_join(p1, NULL);
	pthread_join(p2, NULL);

	psscli_ws_free();

	return 0;
}
