#ifndef PSSCLI_CMD_H_
#define PSSCLI_CMD_H_

#define PSSCLI_CMD_RESPONSE_MAX 4096+256

enum psscli_cmd_code {
	PSSCLI_CMD_NONE,
	PSSCLI_CMD_BASEADDR,
	PSSCLI_CMD_GETPUBLICKEY,
	PSSCLI_CMD_SETPEERPUBLICKEY
};

typedef struct psscli_cmd_ {
	enum psscli_cmd_code code;
	char id;
	char **values;
	unsigned char valuecount;
} psscli_cmd;

typedef struct psscli_response_ {
	char id;
	char done;
	char content[PSSCLI_CMD_RESPONSE_MAX]; 
	int length;
} psscli_response;

psscli_cmd* psscli_cmd_alloc(psscli_cmd *cmd, int valuecount);
void psscli_cmd_free(psscli_cmd *cmd);

#endif // PSSCLI_CMD_H_
