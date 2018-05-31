#ifndef PSSCLI_SERVER_H_
#define PSSCLI_SERVER_H_

#define PSSCLI_SERVER_SOCKET_PATH "/tmp/psscli.sock"
#define PSSCLI_SERVER_SOCKET_BUFFER_SIZE 1024
#define PSSCLI_SERVER_SOCKET_READ_SIZE 128

#include "cmd.h"
#include "ws.h"

int psscli_server_start();
int psscli_server_shift(psscli_response *r);

// \todo change to init function to fill in callback in ws
int psscli_server_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
#endif // PSSCLI_SERVER_H_

