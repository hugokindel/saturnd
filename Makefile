.PHONY: distclean all

CC ?= gcc
CCFLAGS ?= -Wall -std=gnu99 -Iinclude

# TODO: Add `saturnd` to `all` once it is stable.
all: cassini

cassini:
	$(CC) $(CCFLAGS) $(CFLAGS) -DCASSINI -o cassini src/main.c src/reply.c src/request.c src/utils.c

saturnd:
	$(CC) $(CCFLAGS) $(CFLAGS) -DSATURND -o saturnd src/main.c src/reply.c src/request.c src/utils.c

distclean:
	rm cassini saturnd *.o
