#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <pthread.h>

#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "server.h"
#include "ws.h"

extern struct psscli_ws_ psscli_ws;
char psscli_cmd_queue_next_;
char psscli_cmd_queue_last_;

int psscli_server_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	char n;
	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			printf("read (%d) %d -> %d, %p / %p\n", pthread_self(), psscli_cmd_queue_next_, psscli_cmd_queue_last_, &psscli_cmd_queue_next_, &psscli_cmd_queue_last_);
			while (psscli_cmd_queue_next_ != psscli_cmd_queue_last_) {
				psscli_cmd_queue_next_++;
				psscli_cmd_queue_next_ %= PSSCLI_SERVER_CMD_QUEUE_MAX;
				if (psscli_ws_send(&psscli_cmd_queue_[psscli_cmd_queue_next_])) {
					psscli_cmd_queue_next_--;
					psscli_cmd_queue_next_ %= PSSCLI_SERVER_CMD_QUEUE_MAX;
				}
			}
			break;
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			lwsl_notice("err %d\n", reason);
			raise(SIGINT);
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			lwsl_notice("pthread %d\n", pthread_self());
			return pthread_self();
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			lwsl_notice("recv %d!\n", len);
			break;
		default:
			lwsl_notice("%d\n", reason);
	}
	return 0;
}

void psscli_cmd_free(psscli_cmd *cmd) {
	int i;

	if (cmd->values != NULL)  {
		for (i = 0; i < cmd->valuecount; i++) {
			free(cmd->values+i);
		}
		free(cmd->values);
	}
	memset(&cmd, 0, sizeof(psscli_cmd));
}

int psscli_server_start() {
	unsigned int s, s2;
	struct sockaddr_un sl, sr;
	int l;
	struct stat fstat;
	char buf[PSSCLI_SERVER_SOCKET_BUFFER_SIZE];
	char t;

	if (!stat(PSSCLI_SERVER_SOCKET_PATH, &fstat)) {
		unlink(PSSCLI_SERVER_SOCKET_PATH);
		//return 1;
	}
	s = socket(AF_UNIX, SOCK_STREAM, 0);

	sl.sun_family = AF_UNIX;
	strcpy(sl.sun_path, PSSCLI_SERVER_SOCKET_PATH);
	l = strlen(sl.sun_path) + sizeof(sl.sun_family);
	if (bind(s, (struct sockaddr*)&sl, l)) {
		return 1;
	}

	listen(s, 5);

	t = 1;
	while (psscli_ws.pid) {
		int n;
		psscli_cmd *cmd;

		l = sizeof(struct sockaddr_un);
		s2 = accept(s, (struct sockaddr*)&sr, &l);

		n = psscli_cmd_queue_last_ + 1;
		n %= PSSCLI_SERVER_CMD_QUEUE_MAX;
		if (n == psscli_cmd_queue_next_) {
			continue;
		}
		psscli_cmd_queue_last_ = n;
		
		cmd = &psscli_cmd_queue_[psscli_cmd_queue_last_];
		psscli_cmd_free(cmd);
		l = recv(s2, &buf, 1, 0);
		cmd->code = *((unsigned char*)&buf);
		printf("write (%d) %d -> %d, %p / %p\n", pthread_self(), psscli_cmd_queue_next_, psscli_cmd_queue_last_, &psscli_cmd_queue_next_, &psscli_cmd_queue_last_);
		switch (cmd->code) {
			case PSSCLI_CMD_BASEADDR:
				l = write(psscli_ws.notify[1], &t, 1);
				if (l < 0) {
					raise(SIGINT);
				}
				break;
			default:
				psscli_cmd_queue_last_--;
				psscli_cmd_queue_last_ %= PSSCLI_SERVER_CMD_QUEUE_MAX;
		}
	}

	return 0;

}

