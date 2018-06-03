#include <pthread.h>

#include "sync.h"

static pthread_rwlock_t pt_lock_run;
static int running;

void psscli_start() {
	pthread_rwlock_wrlock(&pt_lock_run);
	running = 1;
	pthread_rwlock_unlock(&pt_lock_run);
}

void psscli_stop() {
	pthread_rwlock_wrlock(&pt_lock_run);
	running = 0;
	pthread_rwlock_unlock(&pt_lock_run);
}

int psscli_running() {
	int r;
	pthread_rwlock_rdlock(&pt_lock_run);
	r = running;
	pthread_rwlock_unlock(&pt_lock_run);
	return r;
}
