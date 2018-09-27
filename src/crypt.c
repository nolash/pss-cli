#include <sodium.h>
#include <string.h>
#include <stdlib.h>

#include "crypt.h"
#include "config.h"
#include "error.h"

extern struct psscli_config conf;

PRIVATE psscli_key_connect **keyconnects;
PRIVATE psscli_key **keys;
PRIVATE long keys_count = 0;

// for encrypting local log msgs
PRIVATE char localkey[PSSCLI_CRYPT_LOGKEY_LENGTH];

PRIVATE char master_seed[randombytes_SEEDBYTES];
PRIVATE char have_seed = 0;
PRIVATE unsigned char *gpwd; // global password, used for unlocking seed

PRIVATE void generate_seed();
PRIVATE int open_seed();
PRIVATE void close_seed();
PRIVATE int derived_seed(const unsigned long idx, char seed[crypto_kx_SEEDBYTES]);

// flags = settings
// idx = index of key to derive
// pw = password to unlock stored seed (ignored if newkey)
int psscli_crypt_init(int flags, int idx, char *pwd) {
	if (sodium_init() < 0) {
		return PSSCLI_ENOINIT;
	}
	if ((flags & PSSCLI_CRYPT_NEWKEY) == 0) {
		// open key, if success set
		if (!open_seed()) {
			return PSSCLI_EKEY;
		}
	}
	if (!have_seed) {
		generate_seed();
		randombytes_buf(localkey, PSSCLI_CRYPT_LOGKEY_LENGTH);
	}
	keyconnects = malloc(sizeof(psscli_key_connect**)*PSSCLI_KEY_CAPACITY);
	keys = malloc(sizeof(psscli_key**)*PSSCLI_KEY_CAPACITY);
	gpwd = pwd;

	return 0;
}

unsigned char* getpassword() {
	return gpwd;
}

// idx stores the index of created key, will be mapped to peer
// returns error
// \todo needs to be thread safe
int psscli_crypt_generate_key(int *zIdx) {
	char seed[crypto_kx_SEEDBYTES];
	int r;
	if ((r = derived_seed(keys_count, seed)) != PSSCLI_EOK) {
		return r;
	}
	*(keys+keys_count) = malloc(sizeof(psscli_key));
	memset((*(keys+keys_count))->pub, 0, crypto_kx_PUBLICKEYBYTES);
	memset((*(keys+keys_count))->sec, 0, crypto_kx_SECRETKEYBYTES);
	crypto_kx_seed_keypair((*(keys+keys_count))->pub, (*(keys+keys_count))->sec, seed);		
	keys_count++;
	*zIdx = keys_count;

	return PSSCLI_EOK;
}

// from incoming session key, create outgoing session key and link 
// returns normal error
int psscli_crypt_connect(psscli_publickey remote, psscli_key_connect *c) {
	int newidx;
	int r;

	if ((r = psscli_crypt_generate_key(&newidx))) {
		return r;
	}
	
	memcpy(c->remote, remote, crypto_kx_PUBLICKEYBYTES);
	c->local = *(keys+newidx);

	return PSSCLI_EOK;
}

int psscli_crypt_new_session(psscli_key_connect *c, unsigned char *sessionkey) {
	randombytes_buf(c->in, PSSCLI_CRYPT_SESSIONKEY_LENGTH);
	memcpy(c->out, sessionkey, PSSCLI_CRYPT_SESSIONKEY_LENGTH);
	return PSSCLI_EOK;
}

// get keys by 
int psscli_crypt_get_key(unsigned int idx, psscli_publickey k) {
	memcpy(k, (*(keys+idx))->pub, crypto_kx_PUBLICKEYBYTES);
	return 0;
}

// encrypts and returns ciphertext to target and local copy
// buffers 
int psscli_crypt_encrypt(const psscli_key_connect *c, const unsigned char *msg, const int msglen, unsigned char **zOut, unsigned char **zLocal) {
	unsigned char nonce[crypto_secretbox_NONCEBYTES];
	randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);	
	randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);	
	crypto_secretbox_easy(*zOut, msg, msglen, nonce, c->out);
	crypto_secretbox_easy(*zLocal, msg, msglen, nonce, localkey);
	return PSSCLI_EOK;
}

void psscli_crypt_free() {
	int i;

	for (i = 0; i < keys_count; i++) {
		free(*(keys+i));
	}
	free(keys);
	free(keyconnects);
}

/***
 * \todo endian derive index check
*/
int derived_seed(const unsigned long idx, char seed[crypto_kx_SEEDBYTES]) {
	int copybytes;

	unsigned char hash[crypto_generichash_BYTES];
	unsigned char hashseed[crypto_kx_SEEDBYTES + sizeof(unsigned long)];

	if (open_seed()) {
		return PSSCLI_EKEY;
	}
	memcpy(hashseed, master_seed, crypto_kx_SEEDBYTES);
	close_seed();

	memcpy(hashseed+sizeof(unsigned long), &idx, sizeof(unsigned long));
	if (crypto_generichash(hash, crypto_generichash_BYTES, hashseed, crypto_kx_SEEDBYTES, NULL, 0)) {
		return PSSCLI_EKEY;
	}
	if (crypto_generichash_BYTES < crypto_kx_SEEDBYTES) {
		copybytes = crypto_generichash_BYTES;	
	} else {
		copybytes = crypto_kx_SEEDBYTES;
	}
	memcpy(seed, hash, copybytes);
	return PSSCLI_EOK;
}

// open existing seed
PRIVATE int open_seed() {
	return PSSCLI_EOK;
}

PRIVATE void close_seed() {
}

// new seed
PRIVATE void generate_seed() {
	randombytes_buf((void*)master_seed, randombytes_SEEDBYTES);
}
