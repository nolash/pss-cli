#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>

#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "cmd.h"
#include "server.h"
#include "ws.h"
#include "error.h"
#include "std.h"
#include "config.h"

extern struct psscli_config conf;
extern struct psscli_ws_ psscli_ws;
char psscli_cmd_queue_next_;
char psscli_cmd_queue_last_;
int psscli_response_queue_next_;
int psscli_response_queue_last_;

unsigned int s;
struct sigaction sa_parent;

/***
 * \todo add lws_rx_flow_control() if response queue is full
 * \todo handle send failure with timeout and possible notify
 */
int psscli_server_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int n;
	char *t;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			printf("read (%d) %d -> %d, %p / %p\n", pthread_self(), psscli_cmd_queue_next_, psscli_cmd_queue_last_, &psscli_cmd_queue_next_, &psscli_cmd_queue_last_);
			// send all commands in the queue
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
			psscli_ws.connected = 1;
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			lwsl_notice("err %d (thread %d)\n", reason, pthread_self());
			raise(SIGHUP);
			break;
		case LWS_CALLBACK_CLOSED:
			if (psscli_ws.pid > 0) {
				raise(SIGHUP);
			}
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			lwsl_notice("pthread %d\n", pthread_self());
			return pthread_self();
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			lwsl_notice("part recv: %s\n", in);
			psscli_response *res = &psscli_response_queue_[psscli_response_queue_last_];
			strcpy(res->content + res->length, in);
			res->length += len;

			// rpc json terminates at newline, if we have it we can dispatch to handler 
			t = in;
			while (1) {
				t = strchr(t, 0x7b);
				if (t == 0x0) {
					break;
				}
				t++;
				res->id++;
			}
			t = in;
			while (1) {
				t = strchr(t, 0x7d);
				if (t == 0x0) {
					break;
				}
				t++;
				res->id--;
			}
			if (!res->id) {
			//if (*(char*)(in+len-1) == 0x0a) {
				// null-terminate response string
				*(res->content + res->length) = 0;
				// tell handler this response can be operated on
				res->status = PSSCLI_RESPONSE_STATUS_RECEIVED;
				// initialize next response object in queue so the handler knows where to stop
				psscli_response_queue_last_++;
				psscli_response_queue_last_ %= PSSCLI_SERVER_RESPONSE_QUEUE_MAX;
				psscli_response_queue_[psscli_response_queue_last_].length = 0;
				psscli_response_queue_[psscli_response_queue_last_].done = 0;
				lwsl_notice("recv done (last: %d\tnext: %d): %s\n", psscli_response_queue_last_, psscli_response_queue_next_, res->content);
			}
			break;
		default:
			lwsl_notice("%d\n", reason);
	}
	return 0;
}


void psscli_server_sigint_(int s) {
	close(s);
	sa_parent.sa_handler(SIGINT);
}

int psscli_server_load_queue() {
	psscli_cmd_queue_last_ = 0;
	psscli_cmd_queue_next_ = 0;
	psscli_response_queue_last_ = 0;
	psscli_response_queue_next_ = 0;
	memset(&psscli_cmd_queue_[0], 0, sizeof(psscli_cmd));
	memset(&psscli_response_queue_[0], 0, sizeof(psscli_response));
	return 0;
}

/***
 *
 * \todo issue warning if queue is full
 *
 */
int psscli_server_start() {
	unsigned int s2;
	struct sockaddr_un sl, sr;
	int l;
	struct stat fstat;
	char buf[PSSCLI_SERVER_SOCKET_BUFFER_SIZE];
	char t;
	struct sigaction sa;

	sa.sa_handler = psscli_server_sigint_;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, &sa_parent);

	if (!stat(conf.sock, &fstat)) {
		unlink(conf.sock);
		//return 1;
	}
	s = socket(AF_UNIX, SOCK_STREAM, 0);

	sl.sun_family = AF_UNIX;
	strcpy(sl.sun_path, conf.sock);
	l = strlen(sl.sun_path) + sizeof(sl.sun_family);
	if (bind(s, (struct sockaddr*)&sl, l)) {
		return 1;
	}

	listen(s, 5);

	psscli_server_load_queue();

	while (psscli_ws.pid) {
		int n;
		short *sl[4];
		psscli_cmd *cmd;

		l = sizeof(struct sockaddr_un);
		s2 = accept(s, (struct sockaddr*)&sr, &l);

		while (l = recv(s2, &buf, 1, 0) > 0) {
			int m;
			char *p;

			// next in queue
			n = psscli_cmd_queue_last_ + 1;
			n %= PSSCLI_SERVER_CMD_QUEUE_MAX;

			// noop if queue is full
			if (n == psscli_cmd_queue_next_) {
				n = (unsigned char)PSSCLI_EFULL;
				send(s2, &n, 1, 0); // -1 = queue full
				continue;
			}
			psscli_cmd_queue_last_ = n;
			n *= -1;
	
			// retrieve the command and decide action	
			cmd = &psscli_cmd_queue_[psscli_cmd_queue_last_];
			psscli_cmd_free(cmd);

			p = (unsigned char*)&buf;
			cmd->code = *p;
			cmd->id = *(p+1);

			printf("write (%d) %d -> %d, %p / %p\n", pthread_self(), psscli_cmd_queue_next_, psscli_cmd_queue_last_, &psscli_cmd_queue_next_, &psscli_cmd_queue_last_);

			switch (cmd->code) {
				case PSSCLI_CMD_BASEADDR:
					buf[1] = n;
					l = write(psscli_ws.notify[1], &buf, 2);
					if (l < 0) {
						raise(SIGINT);
					}
					send(s2, &n, 1, 0);
					break;
				case PSSCLI_CMD_GETPUBLICKEY:
					buf[1] = n;
					l = write(psscli_ws.notify[1], &buf, 2);
					if (l < 0) {
						raise(SIGINT);
					}
					send(s2, &n, 1, 0);
					break;
				case PSSCLI_CMD_SETPEERPUBLICKEY:
					// keylen+key 132 bytes + topiclen+topic 10 bytes + addrlen 2 bytes = 144 bytes
					l = recv(s2, &buf, 144, MSG_DONTWAIT);

					// all fields have length prefixes. Get each one
					sl[0] = (short*)&buf;
					if (!is_le()) {
						int16_rev(sl[0]);
					}
					sl[1] = (short*)&buf[2+(*sl[0])];
					if (!is_le()) {
						int16_rev(sl[1]);
					}
					sl[2] = (short*)&buf[4+(*sl[0])+(*sl[1])];
					if (!is_le()) {
						int16_rev(sl[2]);
					}

					// check that we have enough data for required fields
					if (l < 144 || *sl[0] != 130 || *sl[1] != 8) {
						memset(&buf, (char)PSSCLI_EINVAL, 1);
						errno = EPROTO;
						send(s2, &buf, 1, 0);
						continue;
					}
					
					// allocate three vars for this command
					if (psscli_cmd_alloc(cmd, 3) == NULL) {
						memset(&buf, (char)PSSCLI_EMEM, 1);
						errno = ENOMEM;
						send(s2, &buf, 1, 0);
						continue;
					}

					// allocate and set publickey
					*cmd->values = malloc(sizeof(char)*(*sl[0])+3);
					strcpy(*cmd->values, "0x");
					memcpy(*cmd->values+2, &buf[2], *sl[0]);
					memset(*cmd->values+2+(*sl[0]), 0, 1);

					// allocate and set topic
					*(cmd->values+1) = malloc(sizeof(char)*(*sl[1])+3);
					strcpy(*(cmd->values+1), "0x");
					memcpy(*(cmd->values+1)+2, &buf[4+(*sl[0])], *sl[1]);
					memset(*(cmd->values+1)+2+(*sl[1]), 0, 1);

					// if optional address is present, allocate, retrieve and set it
					if (*sl[2] > 0) {
						l = recv(s2, &buf, *sl[2], MSG_DONTWAIT);
						*(cmd->values+2) = malloc(sizeof(char)*(*sl[2])+3);
						strcpy(*(cmd->values+2), "0x");
						memcpy(*(cmd->values+2)+2, &buf, *sl[2]);
						memset(*(cmd->values+2)+2+(*sl[2]), 0, 1);
					}

					// tell the outgoing queue handler about the new pending command
					buf[0] = (unsigned char)PSSCLI_CMD_SETPEERPUBLICKEY;
					l = write(psscli_ws.notify[1], &buf, 1);

					// tell the client all is well
					send(s2, &n, 1, 0);
					break;

				default:
					printf("foo\n");
					psscli_cmd_queue_last_--;
					psscli_cmd_queue_last_ %= PSSCLI_SERVER_CMD_QUEUE_MAX;
			}
		}
	}

	return 0;

}

/***
 * \todo shared memory for response queue cursors
 */	
int psscli_server_shift(psscli_response *r) {
	printf("checking shift\tlast: %d\tnext: %d\n", psscli_response_queue_last_, psscli_response_queue_next_);
	if (psscli_response_queue_last_ == psscli_response_queue_next_) {
		return 1;
	}
	memcpy(r, &psscli_response_queue_[psscli_response_queue_next_], sizeof(psscli_response));
	psscli_response_queue_next_++;
	psscli_response_queue_next_ %= PSSCLI_SERVER_RESPONSE_QUEUE_MAX;
	return 0;
}
