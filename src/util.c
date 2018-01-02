#include <json-c/json.h>
#include <string.h>

#include "psscli.h"
#include "util.h"
#include "error.h"

/***
 *
 * \short load peers from file
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
		array_list *a;

		a = json_object_get_array(array_list_get_idx(ja, i));

		key = json_object_get_string(array_list_get_idx(a, 0));
		addr = json_object_get_string(array_list_get_idx(a, 1));
		nick = json_object_get_string(array_list_get_idx(a, 2));

		psscli_add_peer(c, key, addr, nick);
	}

	return 0;
}
