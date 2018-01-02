#ifndef PSSCLI_SERVER_H_
#define PSSCLI_SERVER_H_

#define PSSCLI_SERVER_SOCKET_PATH "/tmp/psscli.sock"
#define PSSCLI_SERVER_SOCKET_BUFFER_SIZE 1024
#define PSSCLI_SERVER_SOCKET_READ_SIZE 128
#define PSSCLI_SERVER_CMD_QUEUE_MAX 64

#include "ws.h"

// \todo change to init function to fill in callback in ws
int psscli_server_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
psscli_cmd psscli_cmd_queue_[PSSCLI_SERVER_CMD_QUEUE_MAX];
char psscli_cmd_queue_next_;
char psscli_cmd_queue_last_;

int psscli_server_start();

#endif // PSSCLI_SERVER_H_

