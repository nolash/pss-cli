#ifndef PSSCLI_CRYPT_H_
#define PSSCLI_CRYPT_H_

#include <sodium.h>

#include "extra.h"

#define PSSCLI_CRYPT_NEWKEY 1

typedef struct psscli_publickey {
	 unsigned char key[crypto_kx_PUBLICKEYBYTES];
} psscli_publickey;

int psscli_crypt_init(int flags, int idx, char *pw);

// generates a new key, saves it to disk, puts in supplied int pointer
int psscli_crypt_generate_key(char *pw);

int psscli_crypt_get_key(unsigned long idx, psscli_publickey *k);

int psscli_crypt_free_key(unsigned long idx);

#endif // PSSCLI_CRYPT_H_
