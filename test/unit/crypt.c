#include <stdio.h>
#include <sodium.h>

#include "crypt.h"
#include "error.h"

int main() {
	int r;
	int i;

	psscli_publickey k;

	if ((r = psscli_crypt_init(PSSCLI_CRYPT_NEWKEY, 0, NULL)) != PSSCLI_EOK) {
		return 1;
	}

	if ((r = psscli_crypt_generate_key(NULL)) != PSSCLI_EOK) {
		return 2;
	}

	if ((r = psscli_crypt_get_key(0, &k)) != PSSCLI_EOK) {
		return 3;	
	}

	printf("pubkey: 0x");
	for (i = 0; i < crypto_kx_PUBLICKEYBYTES; i++) {
		printf("%02x\n", (unsigned char)k.key[i]);	
	}
	printf("\n");

	psscli_crypt_free_key(0);


	return 0;
}
