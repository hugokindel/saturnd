.PHONY: distclean all

CC ?= gcc
CCFLAGS ?= -Wall -std=c99 -Iinclude

all: cassini saturnd

cassini:
	$(CC) $(CCFLAGS) $(CFLAGS) -DCASSINI -o cassini src/main.c src/timing-text-io.c src/utils.c

saturnd:
	$(CC) $(CCFLAGS) $(CFLAGS) -DSATURND -o saturnd src/main.c src/timing-text-io.c src/utils.c

distclean:
	rm cassini saturnd *.o
