#include <stdio.h>
#include <sodium.h>
#include <string.h>
#include <stdlib.h>

#include "crypt.h"
#include "error.h"

int main() {
	int r;
	int i;

	unsigned char *msg = malloc(sizeof(char)*1024);
	unsigned char *msg_result = malloc(sizeof(char)*1024);
	unsigned char *msgcontent = "Everything looks bad if you remember it";

	int cipherlen = (int)strlen(msgcontent) + crypto_secretbox_MACBYTES;
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

	strcpy(msg, msgcontent);
	if ((r = psscli_crypt_encrypt(&kc, msg, strlen(msg), &msg_out, &msg_local))) {
		return 6;
	}

	printf("pubkey: 0x");
	for (i = 0; i < crypto_kx_PUBLICKEYBYTES; i++) {
		printf("%02x", (unsigned char)k[i]);	
	}
	printf("\nmsg: %s\ncipher: ", msg);

	for (i = 0; i < cipherlen; i++) {
		printf("%02x", (unsigned char)msg_out[i]);	
	}
	printf("\nout local: ");
	for (i = 0; i < cipherlen; i++) {
		printf("%02x", (unsigned char)msg_local[i]);	
	}
	printf("\n");

	(*((unsigned long*)(kc.nonceBase+(crypto_secretbox_NONCEBYTES-sizeof(unsigned long)))))--;
	memcpy(kc.in, kc.out, PSSCLI_CRYPT_SESSIONKEY_LENGTH);
	if ((r = psscli_crypt_decrypt(&kc, msg_out, cipherlen, kc.nonceBase, &msg_result, &msg_local))) {
		return 7;
	}

	printf("---\nplain: %s\nin local: ", msg);
	for (i = 0; i < cipherlen; i++) {
		printf("%02x", (unsigned char)msg_local[i]);	
	}
	printf("\n");

	if (strcmp(msg, msg_result)) {
		return 8;
	}

	free(msg_local);
	free(msg_out);
	free(msg_result);
	free(msg);

	return 0;
}
