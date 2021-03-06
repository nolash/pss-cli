#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>

#include "server.h"
#include "config.h"
#include "cmd.h"
#include "sync.h"
#include "error.h"
#include "extra.h"

// server
static int sd;
static int run;

// client session
int *sdlist[PSSCLI_SERVER_SOCK_MAX]; // maps socket descriptors to socket descriptor pointer values
static int cursor;

// sync
static pthread_rwlock_t pt_rw;
static pthread_t pt_cmd[PSSCLI_SERVER_SOCK_MAX];
static pthread_t pt_reply;
extern pthread_mutex_t pt_lock_queue;
extern pthread_cond_t pt_cond_reply;
extern pthread_cond_t pt_cond_write;

static int running();

int psscli_server_status() {
	if (!run) {
		return PSSCLI_SERVER_STATUS_IDLE;
	}
	return PSSCLI_SERVER_STATUS_RUNNING;
}

// handle socket input command
// returns null if invalid input
psscli_cmd *parse_raw(psscli_cmd *cmd) {
	char b[1024];

	switch (cmd->code) {
		case PSSCLI_CMD_BASEADDR:
			return cmd;
			break;
		case PSSCLI_CMD_GETPUBLICKEY:
			return cmd;
			break;
//		case PSSCLI_CMD_SETPEERPUBLICKEY:
//			// keylen+key 132 bytes + topiclen+topic 10 bytes + addrlen 2 bytes = 144 bytes
//			l = recv(s2, &b, 144, MSG_DONTWAIT);
//
//			// all fields have length prefixes. Get each one
//			sl[0] = (short*)&b;
//			if (!is_le()) {
//				int16_rev(sl[0]);
//			}
//			sl[1] = (short*)&b[2+(*sl[0])];
//			if (!is_le()) {
//				int16_rev(sl[1]);
//			}
//			sl[2] = (short*)&b[4+(*sl[0])+(*sl[1])];
//			if (!is_le()) {
//				int16_rev(sl[2]);
//			}
//
//			// check that we have enough data for required fields
//			if (l < 144 || *sl[0] != 130 || *sl[1] != 8) {
//				memset(&b, (char)PSSCLI_EINVAL, 1);
//				errno = EPROTO;
//				send(s2, &b, 1, 0);
//				continue;
//			}
//			
//			// allocate three vars for this command
//			if (psscli_cmd_alloc(cmd, 3) == NULL) {
//				memset(&b, (char)PSSCLI_EMEM, 1);
//				errno = ENOMEM;
//				send(s2, &b, 1, 0);
//				continue;
//			}
//
//			// allocate and set publickey
//			*cmd->values = malloc(sizeof(char)*(*sl[0])+3);
//			strcpy(*cmd->values, "0x");
//			memcpy(*cmd->values+2, &b[2], *sl[0]);
//			memset(*cmd->values+2+(*sl[0]), 0, 1);
//
//			// allocate and set topic
//			*(cmd->values+1) = malloc(sizeof(char)*(*sl[1])+3);
//			strcpy(*(cmd->values+1), "0x");
//			memcpy(*(cmd->values+1)+2, &b[4+(*sl[0])], *sl[1]);
//			memset(*(cmd->values+1)+2+(*sl[1]), 0, 1);
//
//			// if optional address is present, allocate, retrieve and set it
//			if (*sl[2] > 0) {
//				l = recv(s2, &b, *sl[2], MSG_DONTWAIT);
//				*(cmd->values+2) = malloc(sizeof(char)*(*sl[2])+3);
//				strcpy(*(cmd->values+2), "0x");
//				memcpy(*(cmd->values+2)+2, &b, *sl[2]);
//				memset(*(cmd->values+2)+2+(*sl[2]), 0, 1);
//			}
//
//			// tell the outgoing queue handler about the new pending command
//			b[0] = (unsigned char)PSSCLI_CMD_SETPEERPUBLICKEY;
//			l = write(psscli_ws.notify[1], &b, 1);
//
//			// tell the client all is well
//			send(s2, &n, 1, 0);
//			break;

		default:
			free(*(cmd->values));
			psscli_cmd_free(cmd);	
			return NULL;
	}
}
// thread handling a single socket connection. Adds received commands to the commands queue
static void *process_input(void *arg) {
	int r;
	int c;
	int i;
	int lsd;
	char b[PSSCLI_SERVER_SOCK_BUFFERSIZE];
	psscli_cmd *cmd;

	pthread_rwlock_wrlock(&pt_rw);
	lsd = *((int*)arg);
	pthread_rwlock_unlock(&pt_rw);
	
	while (running()) {
		if ((c = recv(lsd, &b, PSSCLI_SERVER_SOCK_BUFFERSIZE, 0)) <= 0) {
			fprintf(stderr, "input error on %d (%d)\n", lsd, errno);
			shutdown(lsd, SHUT_RDWR);
			close(lsd);
			break;
		}

		fprintf(stderr, "server got: ");
		for (i = 0; i < c; i++) {
			fprintf(stderr, "%02x", b[i]);
		}
		fprintf(stderr, "\n");

		psscli_cmd_alloc(&cmd, 0);

		// lock global vars for copy
		pthread_mutex_lock(&pt_lock_state);
		cmd->id = cursor;
		cmd->sdptr = (int*)arg;
		cmd->status = PSSCLI_STATUS_LOCAL;
		pthread_mutex_unlock(&pt_lock_state);

		// 
		cmd->code = (unsigned char)*b;
		cmd->sd = lsd;
		memcpy(cmd->src, b+1, c-1);
	
		cmd = parse_raw(cmd);
		if (cmd == NULL) {
			fprintf(stderr, "parse error on data from %d (%d)\n", lsd);
			psscli_cmd_free(cmd);
			shutdown(lsd, SHUT_RDWR);
			close(lsd);
			break;
		}

		pthread_mutex_lock(&pt_lock_queue);
		if (psscli_cmd_queue_add(PSSCLI_QUEUE_OUT, cmd) == -1) {
			pthread_mutex_unlock(&pt_lock_queue);
			fprintf(stderr, "cmd queue add fail, id %d\n", cmd->id);
			psscli_cmd_free(cmd);
			shutdown(lsd, SHUT_RDWR);
			close(lsd);
			break;
		} 
		pthread_mutex_unlock(&pt_lock_queue);
		pthread_cond_signal(&pt_cond_write);
	}

	pthread_rwlock_wrlock(&pt_rw);
	sdlist[lsd] = NULL;
	pthread_rwlock_unlock(&pt_rw);
	fprintf(stderr, "exit input process on %d\n", lsd);
	pthread_exit(NULL);
	return NULL;
}

// when triggered polls the response queue and relays ready replies to the respective socket
PRIVATE void *process_reply(void *arg) {
	int r;
	int c;
	int lsd;
	psscli_cmd *cmd;
	psscli_cmd *p;

	//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	fprintf(stderr, "starting process reply thread\n");
	while (running()) {
		pthread_mutex_lock(&pt_lock_queue);
		pthread_cond_wait(&pt_cond_reply, &pt_lock_queue);
		fprintf(stderr, "reply wakeup\n");
		if (!running()) {
			pthread_mutex_unlock(&pt_lock_queue);
			fprintf(stderr, "reply process thread exiting\n");
			break;
		}
		p = psscli_cmd_queue_next(PSSCLI_QUEUE_IN);
		if (p == NULL) {
			pthread_mutex_unlock(&pt_lock_queue);
			continue;
		}
		psscli_cmd_alloc(&cmd, 1);
		psscli_cmd_copy(cmd, p);
		psscli_cmd_free(p); // this frees the command copy in the in queue
		pthread_mutex_unlock(&pt_lock_queue);

		pthread_rwlock_rdlock(&pt_rw);
		if (sdlist[cmd->sd] != cmd->sdptr) {
			psscli_cmd_free(cmd);
			pthread_rwlock_unlock(&pt_rw);
			fprintf(stderr, "stale sd %d (%p) in reply id %d\n", cmd->sd, cmd->sdptr, cmd->id);
			break;
		}
		pthread_rwlock_unlock(&pt_rw);

		fprintf(stderr, "got response id %d: %s\n", cmd->id, *(cmd->values));

		if ((c = send(cmd->sd, *(cmd->values), sizeof(*(cmd->values)), 0)) <= 0) {
			fprintf(stderr, "failed reply socket send on %d (%d)\n", cmd->sd, errno);
		}
		psscli_cmd_free(cmd); // this frees the local copy of the cmd
	}

	pthread_exit(NULL);
	return NULL;
}

int psscli_server_start() {
	int l;
	unsigned int sd2;
	struct sockaddr_un sl, sr;

	struct stat fstat;

	run = 0;

	if(!stat(conf.sock, &fstat)) {
		unlink(conf.sock);
	}
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		sprintf(psscli_error_string, "socket create (%d): %s", errno, conf.sock);
		return PSSCLI_ESOCK;
	}

	sl.sun_family = AF_UNIX;
	strcpy(sl.sun_path, conf.sock);
	l = strlen(sl.sun_path) + sizeof(sl.sun_family);
	if ((bind(sd, (struct sockaddr*)&sl, l)) == -1) {
		sprintf(psscli_error_string, "socket bind on %s: %d", conf.sock, errno);
		return PSSCLI_ESOCK;
	}

	listen(sd, 5);

	run = 1; // we have single thread still

	pthread_create(&pt_reply, NULL, process_reply, NULL);

	l = sizeof(struct sockaddr_un);
	while (sd2 = accept(sd, (struct sockaddr*)&sr, &l)) {
		int r;

		if (!psscli_running()) {
			break;
		} else if (sd2 == -1) {
			fprintf(stderr, "socket %d invalid errno %d, exiting\n", sd, errno);
			break;
		}

		// copy the socket reference for reply
		pthread_rwlock_wrlock(&pt_rw);
		sdlist[sd2] = &sd2;
		pthread_rwlock_unlock(&pt_rw);

		fprintf(stderr, "sock connect %d\n", sd2);

#ifdef TESTING
		int p;
		if (send(sd2, &sdlist[sd2], sizeof(int*), 0) == -1) {
			r = PSSCLI_ESYNC;
			sprintf(psscli_error_string, "failed to send sd ptr in test mode");
		}
		if (send(sd2, &sd2, sizeof(int), 0) == -1) {
			r = PSSCLI_ESYNC;
			sprintf(psscli_error_string, "failed to send sd number in test mode");
		}
#endif

		if (pthread_create(&pt_cmd[r], NULL, process_input, (void*)sdlist[sd2])) { 
			r = PSSCLI_ESYNC;
			fprintf(stderr, "failed to start input processing thread (%d)\n", errno);
			if (send(sd2, (unsigned char*)&r, 1, 0) == -1) {
				sprintf(psscli_error_string, "socket write fail (input processing) on %d (%d)\n", sd2, errno);
			}
		}
		pthread_mutex_lock(&pt_lock_state);
		r = cursor++;
		pthread_mutex_unlock(&pt_lock_state);
	}

	fprintf(stderr, "server shutting down\n");
	pthread_cond_signal(&pt_cond_reply);

	pthread_join(pt_reply, NULL);

	return PSSCLI_EOK;
}

static int running() {
	int r;

	pthread_mutex_lock(&pt_lock_state);
	r = run;
	pthread_mutex_unlock(&pt_lock_state);

	return r;
}

void psscli_server_stop() {
	pthread_mutex_lock(&pt_lock_state);
	run = 0;
	if (shutdown(sd, SHUT_RDWR)) {
		fprintf(stderr, "failed socket shutdown: %d\n", errno);
	}
	close(sd);
	pthread_mutex_unlock(&pt_lock_state);

	return;
}
