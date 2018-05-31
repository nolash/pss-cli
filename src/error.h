#ifndef PSSCLI_ERROR_H_
#define PSSCLI_ERROR_H_

enum psscli_error {
	PSSCLI_EOK,
	PSSCLI_EMEM,
	PSSCLI_ENOINIT,
	PSSCLI_EFULL,
	PSSCLI_EINVAL,
	PSSCLI_ENODATA,
	PSSCLI_ESOCK,
	PSSCLI_EBUSY,
	PSSCLI_EINIT,
};

char psscli_error_string[1024];
char *psscli_get_error_string();

#endif // PSSCLI_ERROR_H_
