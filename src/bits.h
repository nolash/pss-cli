#ifndef PSSCLI_BITS_H_
#define PSSCLI_BITS_H_

#define PSSCLI_PEERS_MAX 1000
#define PSSCLI_PUBKEY_STRLEN 65 * 2 + 2
#define PSSCLI_ADDRESS_STRLEN 32 * 2 + 2
#define PSSCLI_ADDRESS_NONE "0x"

#define PSSCLI_PEERS_SAVEFILENAME "peers.json"

typedef char psscli_address[PSSCLI_ADDRESS_STRLEN + 1];
typedef char psscli_key[PSSCLI_PUBKEY_STRLEN + 1];

typedef struct psscli_peer_ {
	psscli_key key;
	psscli_address address;
	char *nick;
	char *pet;
} psscli_peer;

typedef struct psscli_ {
	int init;
	psscli_peer self;
	psscli_peer *peers;
	int peer_count;
	char version[16];
} psscli;

#endif // PSSCLI_BITS_H_
