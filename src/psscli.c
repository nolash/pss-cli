#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "psscli.h"
#include "error.h"

psscli* psscli_new(void) {
	psscli *c;
	
	c = malloc(sizeof(psscli));
	if (c == NULL) {
		return NULL;
	}
	
	c->self.key[0] = 0x0;
	c->self.address[0] = 0x0;
	c->peer_count = 0;
	if (psscli_check_peer_capacity(c)) {
		free(c);
		return NULL;
	}
	c->init = 1;

	sprintf(c->version, "%d.%d", PSSCLI_VERSION_MAJOR, PSSCLI_VERSION_MINOR);

	return c;
}

int psscli_check_peer_capacity(psscli *c) {
	c->peers = malloc(sizeof(psscli_peer) * PSSCLI_PEERS_MAX);
	if (c->peers == NULL) {
		return 1;
	}
	return 0;
}

enum psscli_error psscli_add_peer(psscli *c, const char *key, const char *addr, const char *nick) {
	psscli_peer *peer;

	if (c == NULL) {
		return PSSCLI_ENOINIT;
	}

	if (PSSCLI_PEERS_MAX <= c->peer_count) {
		return PSSCLI_EFULL;
	}

	peer = c->peers + c->peer_count;
	memset(peer, 0, sizeof(psscli_peer));
	if (nick != NULL) {
		if (strcmp(nick, "")) {
			peer->nick = malloc(sizeof(char) * strlen(nick));
			if (peer->nick == NULL) {
				return PSSCLI_EMEM;
			}
		}
	}

	strcpy(peer->key, key);
	if (addr != NULL) {
		strcpy(peer->address, addr);
	} else {
		strcpy(peer->address, PSSCLI_ADDRESS_NONE);
	}
	if (peer->nick != NULL) {
		strcpy(peer->nick, nick);
	} 

	c->peer_count++;

	return PSSCLI_EOK;
}

void psscli_free(psscli *c) {
	if (c != NULL) {
		int i;

		for (i = 0; i < c->peer_count; i++)  {
			psscli_peer *peer;
		
			peer = c->peers + i;	
			if (peer != NULL) {
				if (peer->nick != NULL) {
					free(peer->nick);
				}	
			}
		}
		free(c);
	}
}

void psscli_version_string(char *str) {
	
}
