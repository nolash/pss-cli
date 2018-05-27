#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "cmd.h"
#include "server.h"

extern struct psscli_config conf;

int main(int argc, char **argv) {
	struct sockaddr_un rs;
	int l;
	int s;
	char b[1024];
	int bsz = 1024;

	struct timespec ts;

	psscli_response res;

	psscli_config_init();
	psscli_config_parse(argc, argv, bsz, b);

	// send command on socket
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "can't instantiate socket (%d)\n", errno);
		return 3;
	}
	rs.sun_family = AF_UNIX;
	strcpy(rs.sun_path, conf.sock);
	l = strlen(rs.sun_path) + sizeof(rs.sun_family);
	if (connect(s, (struct sockaddr *)&rs, l) == -1) {
		fprintf(stderr, "can't connect to socket %s (%d)\n", conf.sock, errno);
		return 3;
	}

	b[0] = PSSCLI_CMD_BASEADDR;
	if (send(s, &b, 1, 0) == -1) {
		fprintf(stderr, "socket send fail (%d)\n", errno);
		return 4;
	}

	// should return 0
	l = recv(s, &b, 1, 0);

	// timeout for polling read/write ready
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	if (psscli_server_init(PSSCLI_SERVER_MODE_CLIENT)) {
		fprintf(stderr, "init fail (%d)\n", errno);
		return 1;
	}

	// poll for response	
	while (psscli_server_shift(&res)) {
		nanosleep(&ts, NULL);
	}

	close(s);
	return 0;
}
