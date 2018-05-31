#include <stdio.h>
#include <string.h>

#include "minq.h"

struct footype {
	int n;
};

struct bartype {
	char *s;
};

int main() {
	int r;
	int i;
	queue_t q;
	void *recv;
	struct footype foo;
	struct bartype bar;
	struct footype foofill[10];

	foo.n = 42;
	bar.s = "xyzzy";

	if (minq_init(&q, 10)) {
		minq_free(&q);
		return 1;	
	}

	if (minq_peek(&q, &recv) != -1) {
		minq_free(&q);
		return 2;	
	}

	if (minq_add(&q, (void*)&foo) != 1 ) {
		minq_free(&q);
		return 3;	
	}

	if (minq_add(&q, (void*)&bar) != 2) {
		minq_free(&q);
		return 4;
	}

	if (minq_peek(&q, &recv) == -1) {
		minq_free(&q);
		fprintf(stderr, "peek fail\n");
		return 5;
	} else if (recv == NULL) {
		fprintf(stderr, "empty return\n");
		minq_free(&q);
		return 5;
	} else if (((struct footype*)(recv))->n != 42) {
		fprintf(stderr, "value mismatch");
		minq_free(&q);
		return 5;
	}

	if (minq_next(&q, &recv) == -1) {
		fprintf(stderr, "next fail\n");
		minq_free(&q);
		return 6;
	} else if (recv == NULL) {
		fprintf(stderr, "empty return\n");
		minq_free(&q);
		return 6;
	} else if (((struct footype*)(recv))->n != 42) {
		fprintf(stderr, "value mismatch\n");
		minq_free(&q);
		return 6;
	}

	if (minq_peek(&q, &recv) == -1) {
		fprintf(stderr, "next fail\n");
		minq_free(&q);
		return 7;
	} else if (recv == NULL) {
		fprintf(stderr, "empty return\n");
		minq_free(&q);
		return 7;
	} else {
		struct bartype *p;
		p = (struct bartype*)(recv);
		if (strcmp(p->s, "xyzzy")) {
			fprintf(stderr, "value mismatch\n");
			minq_free(&q);
			return 7;
		}
	}

	if (minq_next(&q, &recv) == -1) {
		fprintf(stderr, "failed second next");
		minq_free(&q);
		return 8;
	} else if (recv == NULL) {
		fprintf(stderr, "second empty value");
		minq_free(&q);
		return 8;
	} else {
		struct bartype *p;
		p = (struct bartype*)(recv);
		if (strcmp(p->s, "xyzzy")) {
			fprintf(stderr, "second value mismatch\n");
			minq_free(&q);
			return 8;
		}
	}

	for (i = 0; i < 10; i++) {
		foofill[i].n = 43+i;
		r = minq_add(&q, &foofill[i]);
		if (r != 3+i) {
			fprintf(stderr, "failed add 10x at idx %d, returned %d\n", i, r);
			minq_free(&q);
			return 9;
		}
	}

	for (i = 0; i <= 10; i++) {
		r = minq_next(&q, &recv);
		if (i == 10) {
			if (r != -1) {
				fprintf(stderr, "insert beyond capacity\n");
				minq_free(&q);
				return 10;
			}
			break;
		} else if (i < 10 && r) {
			fprintf(stderr, "fail insert\n");
			minq_free(&q);
			return 10;
		}
		fprintf(stderr, "inserting %d\n", 43+i);
		struct footype *p;

		p = (struct footype*)(recv);
		r = 43+i;
		if (p->n != r) {
			fprintf(stderr, "value mismatch; expected %d, got %d\n", r, p->n);
			minq_free(&q);
			return 10;
		}
	}

	fprintf(stderr, "done\n");
	minq_free(&q);

	return 0;
}
