#ifndef PSSCLI_SERVER_H_
#define PSSCLI_SERVER_H_

#define PSSCLI_SERVER_SOCK_MAX 8
#define PSSCLI_SERVER_SOCK_BUFFERSIZE 8096

#define PSSCLI_SERVER_STATUS_IDLE 0
#define PSSCLI_SERVER_STATUS_RUNNING 1

int psscli_server_start();
void psscli_server_stop();
int psscli_server_status();

#endif // PSSCLI_SERVER_H_
