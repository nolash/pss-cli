#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "server.h"
#include "ws.h"

extern struct psscli_config conf;

int *psscli_config_init() {
	memset(&conf, 0, sizeof(conf));
	//memset(conf, 0, sizeof(psscli_config));
	strcpy(conf.host, PSSCLI_WS_DEFAULT_HOST);
	conf.port = PSSCLI_WS_DEFAULT_PORT;
	strcpy(conf.sock, PSSCLI_SERVER_SOCKET_PATH);
	return 0;
}

int psscli_config_parse(int c, char **v, int errLen, char *zErr) {
	char b[32];
	int o;

	memset(b, 0, 32);

	while ((o = getopt(c, v, "h:p:")) != -1) {
		switch(o) {
			case 'h':
				strcpy(conf.host, optarg);
				break;
			case 'p':
				conf.port = (unsigned short)atoi(optarg);
				break;
			case 's':
				strcpy(conf.sock, optarg);
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
