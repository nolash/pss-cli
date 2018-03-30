#ifndef PSSCLI_H_
#define PSSCLI_H_

#define PSSCLI_VERSION_MAJOR 2
#define PSSCLI_VERSION_MINOR 0

#define PSSCLI_CLI_SETPEER_ADDRESS 0
#define PSSCLI_CLI_SETPEER_NICK 1
#define PSSCLI_CLI_SETPEER_PET 2

#include "bits.h"

psscli* psscli_new(void);
int psscli_connect(psscli *c, const char *address, int port);
enum psscli_error psscli_add_peer(psscli *c, const char *key, unsigned int *idx);
enum psscli_error psscli_get_peer(psscli *c, unsigned int idx, psscli_peer *peer);
enum psscli_error psscli_peer_set(psscli *c, unsigned int idx, char item, void *value);
int psscli_check_peer_capacity(psscli *c);
void psscli_free(psscli *c);

#endif // PSSCLI_H_
