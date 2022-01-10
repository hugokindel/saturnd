.PHONY: all distclean cassini saturnd

CC = gcc
CCFLAGS = -Wall -std=gnu99 -Iinclude
COMMONSRC = src/common.c src/reply.c src/request.c src/utils.c
ifeq ($(shell uname),Linux)
	THREADFLAGS = -pthread
endif

all: cassini saturnd

cassini:
	$(CC) $(CCFLAGS) $(COMMONSRC) src/cassini.c -DCASSINI -o cassini

saturnd:
	$(CC) $(CCFLAGS) $(THREADFLAGS) $(COMMONSRC) src/saturnd.c src/worker.c -DSATURND -o saturnd

distclean:
	rm cassini saturnd
