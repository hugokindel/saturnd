.PHONY: distclean all

CC ?= gcc
CCFLAGS ?= -Wall -std=c99 -Iinclude

all: cassini saturnd

cassini:
	$(CC) $(CCFLAGS) $(CFLAGS) -DCASSINI -o cassini src/commandline.c src/main.c src/reply.c src/request.c src/string.c src/timing.c src/utils.c

saturnd:
	$(CC) $(CCFLAGS) $(CFLAGS) -DSATURND -o saturnd src/commandline.c src/main.c src/reply.c src/request.c src/string.c src/timing.c src/utils.c

distclean:
	rm cassini saturnd *.o
