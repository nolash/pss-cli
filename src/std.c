#include "std.h"

void int16_rev(short *s) {
	char *p;
	char x;

	p = (char*)&s;
	x = *p;
	*p = *(p+1);
	*p = x;
}

int is_le() {
	short s;
	char *p;

	s = 1;
	p = (char*)&s;
	if (*p & 1) {
		return 1;
	}
	return 0;
}
