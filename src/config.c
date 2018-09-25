#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>

#include "config.h"
#include "server.h"
#include "ws.h"

extern struct psscli_config conf;

void psscli_config_init() {
	memset(&conf, 0, sizeof(conf));
	strcpy(conf.host, PSSCLI_WS_DEFAULT_HOST);
	conf.port = PSSCLI_WS_DEFAULT_PORT;
	strcpy(conf.sock, PSSCLI_SERVER_SOCK_PATH);
}

int psscli_config_parse(int c, char **v, int errLen, char *zErr) {
	char b[1024];
	int o;
	void *actualarg;


	while ((o = getopt(c, v, "h:p:")) != -1) {
		memset(b, 0, 1024);
		switch(o) {
			case 'h':
				strcpy(conf.host, optarg);
				break;
			case 'p':
				conf.port = strtol(optarg, NULL, 10);
				if (errno) {
					strcpy(zErr, "invalid port");
					return 1;
				}
				break;
			case 's':
				strcpy(conf.sock, optarg);
				break;
			case 'd':
				strcpy(conf.datadir, optarg);
				break;
//			default:
//				sprintf(b, "Invalid argument -%s", o);
//				if (strlen(b) < errLen) {
//					errLen = strlen(b);
//				}
//				strncpy(zErr, b, sz);
//				return -1;
		}
	}

	return 0;
}

// sets pointer r to default value if passed value v is invalid
// returns > 0 if field is invalid
// l is cap of v
int psscli_config_get_default(int field, int l, void *v) {
	return 0;
}
