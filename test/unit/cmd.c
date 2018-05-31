#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"

// test copy from one cmd to another
// second will be allocated and must be freed
int test_copy() {
	psscli_cmd one;
	psscli_cmd other;

	psscli_cmd_alloc(&one, 2);
	psscli_cmd_alloc(&other, 0); 
	*one.values = malloc(sizeof(char)* 32);
	memset(*one.values, 0, 32);
	*(one.values+1) = malloc(sizeof(char)* 16);
	memset(*(one.values+1), 0, 16);
	strcpy(*one.values, "foo");	
	strcpy(*(one.values+1), "bar");

	// check non-allocated destination	
	psscli_cmd_copy(&other, &one);

	if (strcmp(*(one.values), *(other.values))) {
		fprintf(stderr, "value 1 doesn't match, expected '%s' got '%s'\n", *(one.values), *(other.values));
		return 1;
	}

	if (strcmp(*(one.values)+1, *(other.values)+1)) {
		fprintf(stderr, "value 2 doesn't match, expected '%s' got '%s'\n", *(one.values), *(other.values));
		return 2;
	}

	// check if allocated cmd can be reused
	strcpy(*one.values, "baz");
	psscli_cmd_copy(&other, &one);

	if (strcmp(*(one.values), *(other.values))) {
		fprintf(stderr, "value 1 doesn't match, expected '%s' got '%s'\n", *(one.values), *(other.values));
		return 1;
	}

	if (strcmp(*(one.values)+1, *(other.values)+1)) {
		fprintf(stderr, "value 2 doesn't match, expected '%s' got '%s'\n", *(one.values), *(other.values));
		return 2;
	}

	psscli_cmd_free(&other);
	psscli_cmd_free(&one);

	return 0;
}

// test insert and retrieval cmd of queue elements
int test_cmd_queue() {
	int i;
	psscli_cmd cmds[2];
	psscli_cmd *cmdptrs[2];
	psscli_cmd *p;

	if (psscli_queue_start(2, 2)) {
		return 1;	
	}

	psscli_cmd_alloc(&cmds[0], 2);
	psscli_cmd_alloc(&cmds[1], 2);
	cmds[0].code = PSSCLI_CMD_BASEADDR;
	cmds[1].code = PSSCLI_CMD_GETPUBLICKEY;
	cmdptrs[0] = &cmds[0];
	cmdptrs[1] = &cmds[1];
	if (psscli_cmd_queue_add(cmdptrs[0])) {
		return 2;
	}
	if (psscli_cmd_queue_add(cmdptrs[1])) {
		return 3;
	}
	p = psscli_cmd_queue_peek();
	if (p != cmdptrs[0]) {
		return 4;
	} 
	p = psscli_cmd_queue_next();
	if (p != cmdptrs[0]) {
		return 5;
	}
	p = psscli_cmd_queue_peek();
	if (p != cmdptrs[1]) {
		return 6;
	} 

	for (i = 0; i < 2; i++) {
		psscli_cmd_free(cmdptrs[i]);
	}

	psscli_queue_stop();
	return 0;	
}

// test insert and retireval of response queue elements
int test_response_queue() {
	int i;
	psscli_response resp[2];
	psscli_response *respptr[2];
	psscli_response *p;

	psscli_queue_start(2, 2);

	resp[0].id = 13;
	resp[1].id = 42;
	respptr[0] = resp;	
	respptr[1] = resp+1;	

	if (psscli_response_queue_add(respptr[0])) {
		return 1;
	}
	if (psscli_response_queue_add(respptr[1])) {
		return 2;
	}
	p = psscli_response_queue_peek();
	if (p != respptr[0]) {
		return 3;
	}
	p = psscli_response_queue_next();
	if (p != respptr[0]) {
		return 4;
	}
	p = psscli_response_queue_peek();
	if (p != respptr[1]) {
		return 5;
	}

	psscli_queue_stop();
	return 0;
}

int main() {
	if (test_copy()) {
		return 1;
	}
	if (test_cmd_queue()) {
		return 2;
	}
	if (test_response_queue()) {
		return 3;
	}
	return 0;
}
