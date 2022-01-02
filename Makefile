.PHONY: all distclean cassini saturnd

CC = gcc
CCFLAGS = -Wall -std=gnu99 -Iinclude
COMMONSRC = src/reply.c src/request.c src/utils.c

all: cassini saturnd

cassini:
	$(CC) $(CCFLAGS) $(CFLAGS) $(COMMONSRC) src/cassini.c -DCASSINI -o cassini

saturnd:
	$(CC) $(CCFLAGS) $(CFLAGS) $(COMMONSRC) src/saturnd.c -DSATURND -DDAEMONIZE -o saturnd

distclean:
	rm cassini saturnd
