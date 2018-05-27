#ifndef PSSCLI_H_
#define PSSCLI_H_

#define PSSCLI_VERSION_MAJOR 2
#define PSSCLI_VERSION_MINOR 0

#define PSSCLI_PEER_KEY 0
#define PSSCLI_PEER_ADDRESS 1
#define PSSCLI_PEER_NICK 2
#define PSSCLI_PEER_PET 3

#define PSSCLI_PEER_NAMESIZE 256

#include "bits.h"

psscli* psscli_new(void);
int psscli_connect(psscli *c, const char *address, int port);
enum psscli_error psscli_add_peer(psscli *c, const char *key, unsigned int *idx);
enum psscli_error psscli_peer_get(psscli *c, unsigned int idx, char item, void **value);
enum psscli_error psscli_peer_set(psscli *c, unsigned int idx, char item, void *value);
int psscli_check_peer_capacity(psscli *c);
int psscli_peer_count(psscli *c);
void psscli_free(psscli *c);

#endif // PSSCLI_H_
