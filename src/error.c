#include <string.h>

#include "error.h"

extern char psscli_error_string[1024];

char *psscli_get_error_string() {
	return psscli_error_string;
}

void psscli_set_error_string(const char *s) {
	strcpy(psscli_error_string, s);
}
