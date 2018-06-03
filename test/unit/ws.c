#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "ws.h"
#include "error.h"

#define BASEADDR_CMD "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"pss_baseAddr\"}"
#define GETPUBLICKEY_CMD "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"pss_getPublicKey\"}"
#define SETPEERPUBLICKEY_CMD "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"pss_setPeerPublicKey\",\"params\":[\"0xdeadbeef\",\"0xbeeffeed\",\"0x\"]}"

#define BASEADDR_RESULT_INNER "0x1234567890abcdef"
#define BASEADDR_RESULT "{\"jsonrpc\":\"2.0\",\"id\":42,\"result\":\"0x1234567890abcdef\"}"

int json_write(char *json_string, int json_string_len, psscli_cmd *cmd);
int json_parse(psscli_cmd *cmd);

int test_json_parse() {
	psscli_cmd *cmd;

	psscli_ws_init(NULL, "2.0");

	psscli_cmd_alloc(&cmd, 1);
	*(cmd->values) = malloc(sizeof(char)*1024);
	strcpy(cmd->src, BASEADDR_RESULT);
	cmd->status &= 0;

	if (json_parse(cmd) != PSSCLI_ENODATA) {
		return 1;	
	}

	cmd->status |= PSSCLI_STATUS_COMPLETE;
	if (json_parse(cmd)) {
		return 2;
	}

	if (strcmp(*(cmd->values), BASEADDR_RESULT_INNER)) {
		return 3;
	}

	psscli_cmd_free(cmd);

	return 0;
}

int test_json_write() {
	int i;
	psscli_cmd *cmd;
	char *b;

	psscli_ws_init(NULL, "2.0");

	b = malloc(sizeof(char)*1024);
	cmd = psscli_cmd_alloc(&cmd, 0);
	cmd->code = PSSCLI_CMD_BASEADDR;
	json_write(b, 1024, cmd);
	if (strcmp(b, BASEADDR_CMD)) {
		return 1;	
	}

	cmd->code = PSSCLI_CMD_GETPUBLICKEY;
	json_write(b, 1024, cmd);
	if (strcmp(b, GETPUBLICKEY_CMD)) {
		fprintf(stderr, "got %s\n", b);
		return 2;	
	}

	psscli_cmd_free(cmd);
	psscli_cmd_alloc(&cmd, 3);
	cmd->code = PSSCLI_CMD_SETPEERPUBLICKEY;
	for (i = 0; i < 2; i++) {
		*(cmd->values+i) = malloc(sizeof(char)*64);
	}
	strcpy(*(cmd->values), "0xdeadbeef");
	strcpy(*(cmd->values+1), "0xbeeffeed");
	*(cmd->values+2) = NULL;
	json_write(b, 1024, cmd);
	if (strcmp(b, SETPEERPUBLICKEY_CMD)) {
		fprintf(stderr, "got %s\n", b);
		return 3;	
	}

	psscli_cmd_free(cmd);
	free(b);
}

int main() {
	if (test_json_write()) {
		return 1;
	}
	if (test_json_parse()) {
		return 2;
	}
	return 0;
}
