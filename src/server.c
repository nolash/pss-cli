#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/un.h>

#include "cmd.h"
#include "server.h"
#include "ws.h"
#include "error.h"
#include "std.h"
#include "config.h"

static char b[1024];

extern struct psscli_config conf;
extern struct psscli_ws_ psscli_ws;
static char cmd_queue_next;
static char cmd_queue_last;
static int response_queue_next;
static int response_queue_last;
psscli_cmd *cmd_queue;
psscli_response *response_queue;

unsigned int s;
struct sigaction sa_parent;

key_t cmd_queue_shm_key;
int cmd_queue_shm_id;
key_t response_queue_shm_key;
int response_queue_shm_id;

/***
 * \todo add lws_rx_flow_control() if response queue is full
 * \todo handle send failure with timeout and possible notify
 */
int psscli_server_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
	int n;
	char *t;

	fprintf(stderr, "ws callback: reason: %d\tws: %p\n", reason, wsi);
	switch (reason) {
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			fprintf(stderr, "read (%d) %d -> %d, %p / %p\n", pthread_self(), cmd_queue_next, cmd_queue_last, &cmd_queue_next, &cmd_queue_last);
			// send all commands in the queue
			while (cmd_queue_next != cmd_queue_last) {
				cmd_queue_next++;
				cmd_queue_next %= PSSCLI_SERVER_CMD_QUEUE_MAX;
				if (psscli_ws_send(cmd_queue+cmd_queue_next)) {
					cmd_queue_next--;
					cmd_queue_next %= PSSCLI_SERVER_CMD_QUEUE_MAX;
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
			psscli_response *res = response_queue+response_queue_last;
			strcpy(res->content+res->length, in);
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
				res->done = 1;
				// initialize next response object in queue so the handler knows where to stop
				response_queue_last++;
				response_queue_last %= PSSCLI_SERVER_RESPONSE_QUEUE_MAX;
				(response_queue+response_queue_last)->length = 0;
				(response_queue+response_queue_last)->done = 0;
				lwsl_notice("recv done (queue: %p\tlast: %d\tnext: %d): %s\n", response_queue, response_queue_last, response_queue_next, res->content);
			}
			break;
		case 71:
			fprintf(stderr, "foo");
			break;
		default:
			lwsl_notice("%d\n", reason);
	}
	return 0;
}


void psscli_server_sigint_(int s) {
	psscli_server_stop();
	close(s);
	sa_parent.sa_handler(SIGINT);
}

int psscli_server_load_queue() {
	return 0;
}

int psscli_server_init(int mode) {
	int r;
	int lcmd;
	int lresp;
	int fd;
	int fd_perm;

	lcmd = PSSCLI_SERVER_CMD_QUEUE_MAX * (PSSCLI_SERVER_CMD_ITEM_MAX + sizeof(psscli_cmd));
	lresp = PSSCLI_SERVER_RESPONSE_QUEUE_MAX * sizeof(psscli_response);

	fd_perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWOTH;

	if (mode == PSSCLI_SERVER_MODE_DAEMON) {
		fd = open(PSSCLI_SERVER_CMD_SHM_PATH, O_CREAT | O_TRUNC, fd_perm);
		if (fd == -1) {
			fprintf(stderr, "shm cmd file create failed: %d\n", errno);
			raise(SIGINT);
			return 1;
		}
		close(fd);
	}
	cmd_queue_shm_key = ftok(PSSCLI_SERVER_CMD_SHM_PATH, PSSCLI_SERVER_SHM_PROJ);
	if (cmd_queue_shm_key == -1) {
		fprintf(stderr, "shm cmd failed: %d\n", errno);
		raise(SIGINT);
		return 1;
	}
	cmd_queue_shm_id = shmget(cmd_queue_shm_key, lcmd, 0644 | IPC_CREAT);
	if (cmd_queue_shm_id == -1) {
		fprintf(stderr, "shmget cmd failed: %d\n", errno);
		raise(SIGINT);
		return 1;
	}
	cmd_queue = shmat(cmd_queue_shm_id, (void*)0, 0);
	if (cmd_queue == (void*)-1) {
		fprintf(stderr, "shmat cmd failed: %d\n", errno);
		raise(SIGINT);
		return 1;
	}

	if (mode == PSSCLI_SERVER_MODE_DAEMON) {	
		fd = open(PSSCLI_SERVER_RESPONSE_SHM_PATH, O_CREAT | O_TRUNC, fd_perm);
		if (fd == -1) {
			fprintf(stderr, "shm response file create failed: %d\n", errno);
			raise(SIGINT);
			return 1;
		}
		close(fd);
	}
	response_queue_shm_key = ftok(PSSCLI_SERVER_RESPONSE_SHM_PATH, PSSCLI_SERVER_SHM_PROJ);
	if (response_queue_shm_key == -1) {
		fprintf(stderr, "shm cmd failed: %d\n", errno);
		raise(SIGINT);
		return 1;
	}
	response_queue_shm_id = shmget(response_queue_shm_key, lresp, 0640 | IPC_CREAT);
	if (response_queue_shm_id == -1) {
		fprintf(stderr, "shmget response failed: %d\n", errno);
		raise(SIGINT);
		return 1;
	}
	response_queue = shmat(response_queue_shm_id, (void*)0, 0);
	if (response_queue == (void*)-1) {
		fprintf(stderr, "shmat response failed: %d\n", errno);
		raise(SIGINT);
		return 1;
	}

	cmd_queue_last = 0;
	cmd_queue_next = 0;
	response_queue_last = 0;
	response_queue_next = 0;
	memset(cmd_queue, 0, lcmd);
	memset(response_queue, 0, lresp);
	fprintf(stderr, "initialized server (cmd_queue: %p\tresponse_queue: %p\n", cmd_queue, response_queue);
	return 0;

}

int psscli_server_stop() {
	shmdt(cmd_queue);
	shmdt(response_queue);
}

/***
 *
 * \todo issue warning if queue is full
 * \todo fail if already started
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
	}
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		sprintf(psscli_error_string, "socket create %s: %d", conf.sock, errno);
		return PSSCLI_SOCKET;
	}

	sl.sun_family = AF_UNIX;
	strcpy(sl.sun_path, conf.sock);
	l = strlen(sl.sun_path) + sizeof(sl.sun_family);
	if (bind(s, (struct sockaddr*)&sl, l)) {
		sprintf(psscli_error_string, "socket bind on %s: %d", conf.sock, errno);
		return PSSCLI_SOCKET;
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
			n = cmd_queue_last + 1;
			n %= PSSCLI_SERVER_CMD_QUEUE_MAX;

			// noop if queue is full
			if (n == cmd_queue_next) {
				n = (unsigned char)PSSCLI_EFULL;
				send(s2, &n, 1, 0); // -1 = queue full
				continue;
			}
			cmd_queue_last = n;
			n *= -1;
	
			// retrieve the command and decide action	
			cmd = cmd_queue+cmd_queue_last;
			psscli_cmd_free(cmd);

			p = (unsigned char*)&buf;
			cmd->code = *p;
			cmd->id = *(p+1);

			fprintf(stderr, "write (%d) %d -> %d, %p / %p\n", pthread_self(), cmd_queue_next, cmd_queue_last, &cmd_queue_next, &cmd_queue_last);

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
					cmd_queue_last--;
					cmd_queue_last %= PSSCLI_SERVER_CMD_QUEUE_MAX;
			}
		}
	}

	return 0;

}

/***
 * \todo shared memory for response queue cursors
 */	
int psscli_server_shift(psscli_response *r) {
	printf("checking shift (queue: %p\tlast: %d\tnext: %d\n", response_queue, response_queue_last, response_queue_next);
	if (response_queue_last == response_queue_next) {
		return 1;
	}
	memcpy(r, response_queue+response_queue_next, sizeof(psscli_response));
	response_queue_next++;
	response_queue_next %= PSSCLI_SERVER_RESPONSE_QUEUE_MAX;
	return 0;
}
