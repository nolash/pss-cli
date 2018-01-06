#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include "server.h"
#include "ws.h"

#define TESTKEY "049cbde8b50e5420055ee1ee174dc1e61a8388933d12c66ad0e4e45bbe76cc93cd0c940d586f95bb36b65ae9241e1e57b54c8684f44c3a82351c892750afcb36f000abcdef"

extern struct psscli_ws_ psscli_ws;
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

int main() {
	int r;
	int s, s2;
	int l;
	struct sigaction sa;
	char c[1024];
	struct sockaddr_un rs;
	psscli_response res;
	struct timespec ts;

	// initialize websocket params
	psscli_ws.host = "localhost";
	psscli_ws.origin = psscli_ws.host;
	psscli_ws.port = 8546;
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

	// send command on socket
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return 3;
	}
	rs.sun_family = AF_UNIX;
	strcpy(rs.sun_path, psscli_server_socket_path());
	l = strlen(rs.sun_path) + sizeof(rs.sun_family);
	if (connect(s, (struct sockaddr *)&rs, l) == -1) {
		return 3;
	}

	c[0] = 1;
	if (send(s, &c, 1, 0) == -1) {
		return 4;
	}

	// poll for response	
	while (psscli_server_shift(&res)) {
		nanosleep(&ts, NULL);
	}

	c[0] = 2;	
	if (send(s, &c, 1, 0) == -1) {
		return 4;
	}

	while (psscli_server_shift(&res)) {
		nanosleep(&ts, NULL);
	}

	c[0] = 3;
	strcpy(&c[1], TESTKEY);
	if (send(s, &c, 139, 0) == -1) {
		return 4;
	}
	l = recv(s, &c, 1, 0);
	printf("got: %x %d\n", *(unsigned char*)&c, l);
	close(s);

	// responses received, shutdown
	raise(SIGINT);
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
	psscli_ws_free();

	return 0;
}
