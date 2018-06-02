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

// server
static int sd;
static int run;

// client session
static int sdlist[PSSCLI_SERVER_SOCK_MAX]; // sockets descriptors indexed on socket connection id
static int idlist[PSSCLI_SERVER_SOCK_MAX]; // socket connection ids indexed on ws session id
static int cursor;

// sync
static pthread_rwlock_t pt_rw;
static pthread_t pt_cmd[PSSCLI_SERVER_SOCK_MAX];
static pthread_t pt_reply;
extern pthread_mutex_t pt_lock_queue;
extern pthread_cond_t pt_cond_reply;

int psscli_server_status() {
	if (!run) {
		return PSSCLI_SERVER_STATUS_IDLE;
	}
	return PSSCLI_SERVER_STATUS_RUNNING;
}

// thread handling a single socket connection. Adds received commands to the commands queue
static void *process_input(void *arg) {
	int id;
	int r;
	int c;
	int i;
	int lsd;
	char b[PSSCLI_SERVER_SOCK_BUFFERSIZE];
	psscli_cmd *cmd;

	pthread_rwlock_rdlock(&pt_rw);
	memcpy(&id, (int*)arg, sizeof(int));
	lsd = sdlist[id];
	fprintf(stderr, "enter cmd process on %d\n", lsd);

	pthread_rwlock_unlock(&pt_rw);

	pthread_mutex_lock(&pt_lock_state);
	r = run;
	pthread_mutex_unlock(&pt_lock_state);

	while (r) {
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
		} 
		idlist[cmd->id] = id; // now the cmd->id contains the ws seq id
	}

	pthread_rwlock_unlock(&pt_rw);
	free(cmd);
	pthread_exit(NULL);
	return NULL;
}

// when triggered polls the response queue and relays ready replies to the respective socket
void *process_reply(void *arg) {
	int r;
	int c;
	int lsd;
	psscli_response response;
	psscli_response *p;

	pthread_mutex_lock(&pt_lock_state);
	r = run;
	pthread_mutex_unlock(&pt_lock_state);

	fprintf(stderr, "starting process reply thread\n");
	while (r) {
		pthread_mutex_lock(&pt_lock_queue);
		pthread_cond_wait(&pt_cond_reply, &pt_lock_queue);
		if (!run) {
			fprintf(stderr, "reply process thread exiting\n");
			break;
		}
		p = psscli_response_queue_next();
		if (p == NULL) {
			pthread_mutex_unlock(&pt_lock_queue);
			continue;
		}
		memcpy(&response, p, sizeof(psscli_response));
		r = run;
		lsd = sdlist[idlist[response.id]];
		pthread_mutex_unlock(&pt_lock_queue);
		fprintf(stderr, "got response: %s\n", response.content);

		if ((c = send(lsd, response.content, response.length, 0)) <= 0) {
			fprintf(stderr, "failed reply socket send on %d (%d)\n", lsd, errno);
			continue;
		}
	}

	pthread_exit(NULL);
	return NULL;
}

int psscli_server_start() {
	int l;
	unsigned int sd2;
	struct sockaddr_un sl, sr;
	int idxlist[PSSCLI_SERVER_SOCK_MAX]; // indices for socket descriptor for copy to process input 

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

	listen(sd, 5);

	pthread_mutex_lock(&pt_lock_state);
	run = 1;
	pthread_mutex_unlock(&pt_lock_state);

	pthread_create(&pt_reply, NULL, process_reply, NULL);

	l = sizeof(struct sockaddr_un);
	while (sd2 = accept(sd, (struct sockaddr*)&sr, &l)) {
		int r;

		pthread_rwlock_rdlock(&pt_rw);
		if (!run) {
			pthread_rwlock_unlock(&pt_rw);
			break;
		}
		sdlist[cursor] = sd2;
		idxlist[cursor] = cursor;
		pthread_rwlock_unlock(&pt_rw);

		fprintf(stderr, "sock connect %d\n", sd2);
		
		if (pthread_create(&pt_cmd[cursor], NULL, process_input, (void*)&idxlist[cursor])) { 
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
	pthread_cond_signal(&pt_cond_reply);
	return;
}
