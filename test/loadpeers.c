#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "psscli.h"
#include "test.h"
#include "error.h"

int main() {
	psscli *c;
	enum psscli_error e;
	unsigned char *value;

	c = psscli_new();
	if (c == 0x0) {
		return 2;
	}
	e = psscli_peers_load(c, "test");
	if (e < 0) {
		return 1;
	}

	value = malloc(sizeof(char)*1024);

 	psscli_peer_get(c, 0, PSSCLI_PEER_KEY, (void*)&value);
	fprintf(stderr, "%s\n", value);

	e = psscli_peers_save(c, "test/tmp");
	free(value);
	psscli_free(c);
	return 0;
}
