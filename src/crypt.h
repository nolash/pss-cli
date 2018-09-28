#ifndef PSSCLI_CRYPT_H_
#define PSSCLI_CRYPT_H_

#include <sodium.h>

#include "extra.h"

#define PSSCLI_CRYPT_NEWKEY 1
#define PSSCLI_CRYPT_SESSIONKEY_LENGTH crypto_secretbox_KEYBYTES
#define PSSCLI_CRYPT_LOGKEY_LENGTH crypto_secretbox_KEYBYTES
#define PSSCLI_KEY_CAPACITY 1024

typedef struct psscli_key {
	unsigned char pub[crypto_kx_PUBLICKEYBYTES];
	unsigned char sec[crypto_kx_PUBLICKEYBYTES];
} psscli_key;

typedef unsigned char psscli_publickey[crypto_kx_PUBLICKEYBYTES];

typedef struct psscli_key_connect {
	psscli_publickey remote;
	unsigned char in[PSSCLI_CRYPT_SESSIONKEY_LENGTH];
	unsigned char out[PSSCLI_CRYPT_SESSIONKEY_LENGTH];
	psscli_key *local;
	unsigned char nonceBase[crypto_secretbox_NONCEBYTES];
	unsigned long nonceBaseTail;
} psscli_key_connect;

typedef struct psscli_log {
	unsigned char *msg;
	unsigned long nonceInc;
} psscli_log;


int psscli_crypt_init(int flags, int idx, char *pw);

// generates a new key, saves it to disk, puts in supplied int pointer
int psscli_crypt_generate_key(int *zIdx);

int psscli_crypt_get_key(unsigned int idx, psscli_publickey k);

int psscli_crypt_connect(psscli_publickey remote, psscli_key_connect *c);

int psscli_crypt_new_session(psscli_key_connect *c, unsigned char *sessionkey);

int psscli_crypt_encrypt(const psscli_key_connect *c, const unsigned char *msg, const int msglen, unsigned char **zOut, unsigned char **zLocal);

int psscli_crypt_decrypt(const psscli_key_connect *c, const unsigned char *msg, const int msglen, unsigned char *nonce, unsigned char **zClearOut, unsigned char **zLocal);

void psscli_crypt_free();

#endif // PSSCLI_CRYPT_H_
