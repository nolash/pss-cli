#include <stdio.h>
#include <sodium.h>
#include <string.h>
#include <stdlib.h>

#include "crypt.h"
#include "error.h"

int main() {
	int r;
	int i;
	char *msg = "Everything looks bad if you remember it";
	int cipherlen = (int)strlen(msg) + crypto_secretbox_MACBYTES;
	unsigned char *msg_out = malloc(sizeof(char)*cipherlen);
	unsigned char *msg_local = malloc(sizeof(char)*cipherlen);

	int keyidx;
	psscli_publickey k;
	psscli_key_connect kc;
	unsigned char sessionkey[PSSCLI_CRYPT_SESSIONKEY_LENGTH];

	if ((r = psscli_crypt_init(PSSCLI_CRYPT_NEWKEY, 0, NULL)) != PSSCLI_EOK) {
		return 1;
	}

	if ((r = psscli_crypt_generate_key(&keyidx)) != PSSCLI_EOK) {
		return 2;
	}

	if ((r = psscli_crypt_get_key(0, k)) != PSSCLI_EOK) {
		return 3;	
	}


	psscli_crypt_free();

	if ((r = psscli_crypt_connect(k, &kc))) {
		return 4;	
	}

	randombytes_buf(sessionkey, PSSCLI_CRYPT_SESSIONKEY_LENGTH);
	if ((r = psscli_crypt_new_session(&kc, sessionkey))) {
		return 5;
	}

	if ((r = psscli_crypt_encrypt(&kc, msg, strlen(msg), &msg_out, &msg_local))) {
		return 6;
	}

	printf("pubkey: 0x");
	for (i = 0; i < crypto_kx_PUBLICKEYBYTES; i++) {
		printf("%02x", (unsigned char)k[i]);	
	}
	printf("\nmsg: %s\nout: ", msg);

	for (i = 0; i < cipherlen; i++) {
		printf("%02x", (unsigned char)msg_out[i]);	
	}
	printf("\nlocal: ");
	for (i = 0; i < cipherlen; i++) {
		printf("%02x", (unsigned char)msg_local[i]);	
	}
	printf("\n");


	free(msg_out);
	free(msg_local);

	return 0;
}
