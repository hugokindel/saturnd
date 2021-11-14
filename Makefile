# TODO: Créer/nettoyer l'exécutable saturnd
# TODO: Compiler automatiquement tous les .c

.PHONY: distclean all

CC ?= gcc
CCFLAGS ?= -Wall -std=c99  -Iinclude

all: cassini

cassini:
	$(CC) $(CCFLAGS) $(CFLAGS) -o cassini src/cassini.c src/timing-text-io.c

distclean:
	rm cassini *.o
