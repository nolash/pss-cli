#ifndef PSSCLI_CONFIG_H_
#define PSSCLI_CONFIG_H_

struct psscli_config {
	char host[15];
	unsigned short port;
	char sock[512];
} conf;

int *psscli_config_init();
int psscli_config_load(const char *path);
int psscli_config_parse(int c, char **v, int errLen, char *zErr);

#endif // PSSCLI_CONFIG_H_
