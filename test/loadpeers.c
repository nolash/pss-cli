#include "util.h"
#include "psscli.h"
#include "test.h"
#include "error.h"

int main() {
	psscli *c;
	enum psscli_error e;

	c = psscli_new();
	if (c == 0x0) {
		return 2;
	}
	e = psscli_peers_load(c, "test");
	if (e < 0) {
		return 1;
	}
	psscli_free(c);
	return 0;
}
