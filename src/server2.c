#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>

#include "server2.h"
#include "config.h"
#include "cmd.h"
#include "sync.h"
#include "error.h"

static pthread_rwlock_t pt_rw;

static int sd;

static pthread_t pt_cmd[PSSCLI_SERVER_SOCK_MAX];
static int sdlist[PSSCLI_SERVER_SOCK_MAX];
static int idlist[PSSCLI_SERVER_SOCK_MAX];
static int cursor;

static int run;

int psscli_server_status() {
	if (!run) {
		return PSSCLI_SERVER_STATUS_IDLE;
	}
	return PSSCLI_SERVER_STATUS_RUNNING;
}

static void *process_input(void *arg) {
	int id;
	int r;
	int c;
	int i;
	int lsd;
	char b[PSSCLI_SERVER_SOCK_BUFFERSIZE];
	psscli_cmd *cmd;

	memcpy(&id, (int*)arg, sizeof(int));

	pthread_rwlock_rdlock(&pt_rw);
	lsd = sdlist[id];
	pthread_rwlock_unlock(&pt_rw);

	while (lsd > -1) {
		if ((c = recv(lsd, &b, PSSCLI_SERVER_SOCK_BUFFERSIZE, 0)) <= 0) {
			fprintf(stderr, "input error on %d (%d)\n", lsd, errno);
			shutdown(lsd, SHUT_RDWR);
			close(sdlist[id]);
			break;
		}

		fprintf(stderr, "server got: ");
		for (i = 0; i < c; i++) {
			fprintf(stderr, "%02x", b[i]);
		}
		fprintf(stderr, "\n");

		cmd = malloc(sizeof(psscli_cmd));
		psscli_cmd_alloc(cmd, 1);
		*(cmd->values) = malloc(sizeof(unsigned char)*(c-1));

		cmd->code = (unsigned char)*b;
		cmd->id = cursor;
		memcpy(*(cmd->values), b+1, c-1);
		
		fprintf(stderr, "process input at %p: %s\n", (cmd->values), *(cmd->values));
		free(*(cmd->values));

		pthread_rwlock_wrlock(&pt_rw);
		if (psscli_cmd_queue_add(cmd) == -1) {
			shutdown(sdlist[id], SHUT_RDWR);
			fprintf(stderr, "cmd queue add fail, id %d\n", cmd->id);
		} else {
			idlist[id] = cmd->id; // now the cmd->id contains the ws seq id
			if (psscli_cmd_queue_next() == NULL){
				shutdown(sdlist[r], SHUT_RDWR);
				fprintf(stderr, "cmd queue retrieve fail, id %d\n", cmd->id);
			} else if (send(sdlist[id], &idlist[id], 4, 0) <= 0) {
				shutdown(sdlist[id], SHUT_RDWR);
				fprintf(stderr, "sock result send fail, sd %d, id %d\n", sdlist[r], r);
			}
		}
	}

	pthread_rwlock_unlock(&pt_rw);
	free(cmd);
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
	if (bind(sd, (struct sockaddr*)&sl, l)) {
		sprintf(psscli_error_string, "socket bind on %s: %d", conf.sock, errno);
		return PSSCLI_ESOCK;
	}

	run = 1;
	listen(sd, 5);

	l = sizeof(struct sockaddr_un);
	while (sd2 = accept(sd, (struct sockaddr*)&sr, &l)) {
		int r;

		pthread_rwlock_rdlock(&pt_rw);
		if (!run) {
			pthread_rwlock_unlock(&pt_rw);
			break;
		}
		sdlist[cursor] = sd2;
		r = cursor;
		pthread_rwlock_unlock(&pt_rw);

		fprintf(stderr, "sock connect %d\n", sd2);
		
		if (pthread_create(&pt_cmd[cursor], NULL, process_input, (void*)&cursor)) { 
			r = PSSCLI_ESYNC;
			sprintf(psscli_error_string, "failed to start input processing thread (%d)\n", errno);
			if (send(sd2, (unsigned char*)&r, 1, 0) == -1) {
				sprintf(psscli_error_string, "socket write fail (input processing) on %d (%d)\n", sd2, errno);
			}
		}
		cursor++;
	}

	run = 0;
	close(sd);
	return PSSCLI_EOK;
}

void psscli_server_stop() {
	run = 0;
	if (shutdown(sd, SHUT_RDWR)) {
		fprintf(stderr, "failed socket shutdown: %d\n", errno);
	}
	close(sd);
	return;
}
