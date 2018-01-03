#ifndef PSSCLI_H_
#define PSSCLI_H_

#define PSSCLI_VERSION_MAJOR 2
#define PSSCLI_VERSION_MINOR 0

#include "bits.h"

psscli* psscli_new(void);
int psscli_connect(psscli *c, const char *address, int port);
enum psscli_error psscli_add_peer(psscli *c, const char *key, const char *addr, const char *nick);
int psscli_check_peer_capacity(psscli *c);
void psscli_free(psscli *c);

#endif // PSSCLI_H_
