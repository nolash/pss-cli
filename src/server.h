#ifndef PSSCLI_SERVER_H_
#define PSSCLI_SERVER_H_

#define PSSCLI_SERVER_SOCKET_PATH "/tmp/pssclisock"
#define PSSCLI_SERVER_SOCKET_BUFFER_SIZE 1024
#define PSSCLI_SERVER_SOCKET_READ_SIZE 128
#define PSSCLI_SERVER_SHM_PROJ 922
#define PSSCLI_SERVER_CMD_SHM_PATH "/tmp/psscli-cmd.shm"
#define PSSCLI_SERVER_CMD_QUEUE_MAX 64
#define PSSCLI_SERVER_CMD_ITEM_MAX 8192
#define PSSCLI_SERVER_RESPONSE_SHM_PATH "/tmp/psscli-response.shm"
#define PSSCLI_SERVER_RESPONSE_QUEUE_MAX 64

#define PSSCLI_SERVER_MODE_DAEMON 0
#define PSSCLI_SERVER_MODE_CLIENT 1

#include "cmd.h"
#include "ws.h"

int psscli_server_init(int mode);
int psscli_server_start();
int psscli_server_stop();
int psscli_server_shift(psscli_response *r);

// \todo change to init function to fill in callback in ws
int psscli_server_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

#endif // PSSCLI_SERVER_H_

