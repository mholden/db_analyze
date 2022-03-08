#include <stdio.h>
#include <getopt.h>

#include "innodb_analyze.h"

int main(int argc, char **argv) {
	int ch, idb = 0;
	int err = 0;
	
	struct option longopts[] = {
		{ "idb",	no_argument,	NULL,   'i' },
		{ NULL,				0,		NULL,	0 }
	};

	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (ch) {
			case 'i':
                idb = 1;
				break;
			default:
				printf("usage: %s --idb <database file>\n", argv[0]);
				return -1;
		}
	}
    
    if (!idb) {
        printf("usage: %s --idb <database file> <system-space-file>\n", argv[0]);
        return -1;
    }

	argc--;
	argv++;	
	
	err = innodb_analyze(argc, argv);
    
	return err;
}
