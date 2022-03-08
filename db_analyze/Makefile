CC=gcc
CFLAGS=-g
INCLUDES=-I./innodb

all: db_analyze

db_analyze: db_analyze.o innodb_analyze.o
	$(CC) $(CFLAGS) db_analyze.o innodb_analyze.o -o db_analyze

innodb_analyze: db_analyze.o innodb_analyze.o
	$(CC) $(CFLAGS) db_analyze.o innodb_analyze.o -o db_analyze

db_analyze.o: db_analyze.c
	$(CC) $(INCLUDES) $(CFLAGS) -c db_analyze.c

innodb_analyze.o: innodb/innodb_analyze.c
	$(CC) $(CFLAGS) -c innodb/innodb_analyze.c

clean:
	rm *o db_analyze

install:
	cp db_analyze $(HOME)/devel/install/

.PHONY: all clean install
