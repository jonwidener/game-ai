#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "chess.h"
#include "db.h"
#include "mem.h"

int main() {
	int rc;

	rc = init_db();
	printf("RC: %d\n", rc);

	return 0;
}
