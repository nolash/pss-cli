#include <sodium.h>
#include <string.h>
#include <stdlib.h>

#include "crypt.h"
#include "config.h"
#include "error.h"

extern struct psscli_config conf;

PRIVATE char **publickeys; // randombytes_kx_PUBLICKEYBYTES;
PRIVATE char **privatekeys; // [randombytes_kx_PRIVATEKEYBYTES];
PRIVATE long keys_count = 0;
PRIVATE char master_seed[randombytes_SEEDBYTES];
PRIVATE char have_seed = 0;

PRIVATE void generate_seed();
PRIVATE int open_seed();
PRIVATE void close_seed();
PRIVATE int derived_seed(const unsigned long idx, char *pw, char seed[crypto_kx_SEEDBYTES]);

// flags = settings
// idx = index of key to derive
// pw = password to unlock stored seed (ignored if newkey)
int psscli_crypt_init(int flags, int idx, char *pw) {
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
	}
	publickeys = malloc(sizeof(**publickeys)*1024);
	privatekeys = malloc(sizeof(**publickeys)*1024);
	return 0;
}

// idx is index of key to create, will be mapped to peer
// \todo needs to be thread safe
int psscli_crypt_generate_key(char *pw) {
	char seed[crypto_kx_SEEDBYTES];
	int r;
	if ((r = derived_seed(keys_count, pw, seed)) != PSSCLI_EOK) {
		return r;
	}
	*(publickeys+keys_count) = malloc(crypto_kx_PUBLICKEYBYTES);
	*(privatekeys+keys_count) = malloc(crypto_kx_SECRETKEYBYTES);
	memset(*(publickeys+keys_count), 0, crypto_kx_PUBLICKEYBYTES);
	memset(*(privatekeys+keys_count), 0, crypto_kx_SECRETKEYBYTES);
	crypto_kx_seed_keypair(*(publickeys+keys_count), *(privatekeys+keys_count), seed);		
	keys_count++;

	return PSSCLI_EOK;
}

// \todo fill the empty array space
int psscli_crypt_free_key(unsigned long idx) {
	free(publickeys+idx);
	free(privatekeys+idx);
}

int psscli_crypt_get_key(unsigned long idx, psscli_publickey *k) {
	memcpy(k->key, *(publickeys+idx), crypto_kx_PUBLICKEYBYTES);
	return 0;
}

/***
 * \todo endian derive index check
*/
int derived_seed(const unsigned long idx, char *pw, char seed[crypto_kx_SEEDBYTES]) {
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
