#include <sys/wait.h>
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <signal.h>

#include "server.h"
#include "ws.h"

extern struct psscli_ws_ psscli_ws;
pid_t cpid;

void croak(int sig) {
	psscli_ws.pid = 0;
	printf("killing %d\n", cpid);
	kill(cpid, SIGINT);
	wait(NULL);
}

int main() {
	pid_t pid;
	struct sigaction sa;

	psscli_ws.host = "localhost";
	psscli_ws.origin = psscli_ws.host;
	psscli_ws.port = 8546;
	psscli_ws.ssl = 0;
	if (psscli_ws_init(psscli_server_cb)) {
		return 1;
	}

	pid = fork();

	if (!pid) {
		if (psscli_server_start()) {
		}
		printf("child err: %d\n", errno);		
		return 0;
	}

	cpid = pid;

	sa.sa_handler = croak;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	psscli_ws_connect();
	psscli_ws_free();

	return 0;
}
