#ifndef PSSCLI_SYNC_H_
#define PSSCLI_SYNC_H_

// sync
pthread_t pt_parse;
pthread_t pt_write;

pthread_cond_t pt_cond_parse;
pthread_cond_t pt_cond_write;
pthread_cond_t pt_cond_reply;

pthread_mutex_t pt_lock_state;
pthread_mutex_t pt_lock_queue;

#endif // PSSCLI_SYNC_H_
