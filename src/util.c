#include <json-c/json.h>
#include <string.h>

#include "psscli.h"
#include "util.h"
#include "error.h"

/***
 *
 * \short load peers from file
 *
 * \param c psscli object
 * \param path directory to peers file to load from
 * \return negative value on failure, 0 on success
 */
int psscli_peers_load(psscli *c, const char *path) {
	json_object *jo;
	array_list *ja;
	int r;
	int i;
	char file[strlen(path) + strlen(PSSCLI_PEERS_SAVEFILENAME) + 2];

	strcpy(file, path);
	strcpy(file + strlen(path), "/");
	strcpy(file + strlen(path) + 1, PSSCLI_PEERS_SAVEFILENAME);
	*(file + strlen(path) + strlen(PSSCLI_PEERS_SAVEFILENAME) + 1) = 0;

	jo = json_object_from_file(file);
	if (jo == NULL) {
		return -1;
	}

	ja = json_object_get_array(jo);	
	for (i = 0; i < array_list_length(ja); i++) {
		const char *key;
		const char *addr;
		const char *nick;
		const char *pet;
		array_list *a;
		unsigned int peeridx;
		psscli_peer peer;

		a = json_object_get_array(array_list_get_idx(ja, i));

		key = json_object_get_string(array_list_get_idx(a, 0));
		addr = json_object_get_string(array_list_get_idx(a, 1));
		nick = json_object_get_string(array_list_get_idx(a, 2));
		pet = json_object_get_string(array_list_get_idx(a, 3));

		psscli_add_peer(c, key, &peeridx);
		psscli_peer_set(c, peeridx, PSSCLI_PEER_ADDRESS, (void*)addr);
		psscli_peer_set(c, peeridx, PSSCLI_PEER_NICK, (void*)nick);
		psscli_peer_set(c, peeridx, PSSCLI_PEER_PET, (void*)pet);
	}

	return 0;
}

/***
 *
 * \short save peers to permanent storage
 *
 * \param c the psscli object
 * \param path directory to save file in. 
 * \return negative value on failure
 */
int psscli_peers_save(psscli *c, const char *path) {
	int i;
	json_object *jo;
	char file[strlen(path) + strlen(PSSCLI_PEERS_SAVEFILENAME) + 2];

	strcpy(file, path);
	strcpy(file + strlen(path), "/");
	strcpy(file + strlen(path) + 1, PSSCLI_PEERS_SAVEFILENAME);
	*(file + strlen(path) + strlen(PSSCLI_PEERS_SAVEFILENAME) + 1) = 0;

	jo = json_object_new_array();

	for (i = 0; i < c->peer_count; i++) {
		json_object *o, *okey, *oaddress, *onick, *opet;
		psscli_peer *peer;

		peer = (c->peers)+i;
		o = json_object_new_array();

		okey = json_object_new_string(peer->key);
		json_object_array_add(o, okey);
		oaddress = json_object_new_string(peer->address);
		json_object_array_add(o, oaddress);
		onick = json_object_new_string(peer->nick);
		json_object_array_add(o, onick);
		opet = json_object_new_string(peer->pet);

		json_object_array_add(o, opet);

		json_object_array_add(jo, o);
	}

	if (json_object_to_file(file, jo)) {
		return -1;
	}

	return 0;
}
