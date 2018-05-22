#ifndef PSSCLI_UTIL_H_
#define PSSCLI_UTIL_H_

#include "psscli.h"
#include "bits.h"

int psscli_peers_load(psscli *c, const char* path);
int psscli_peers_save(psscli *c, const char* path);

#endif // PSSCLI_UTIL_H_
