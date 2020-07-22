CFLAGS := -Wall -Werror

# Do not directly rely on dependency files
.PHONY: all clean
.PHONY: debug

all: bin/queue.o bin/trie.o bin/scanWorker.o bin/csteg.o csteg.bin

debug: CFLAGS += -DTESTING -g
debug: clean all

# Use -c option since dependencies of final product
# don't need immediate linking
bin/queue.o: src/queue.c src/queue.h
	gcc -c $(CFLAGS) -o $@ src/queue.c

bin/trie.o: src/trie.c src/trie.h src/queue.h src/csteg.h
	gcc -c $(CFLAGS) -o $@ src/trie.c

bin/scanWorker.o: src/scanWorker.c src/scanWorker.h src/csteg.h src/trie.h
	gcc -c $(CFLAGS) -o $@ src/scanWorker.c

bin/csteg.o: src/csteg.c src/csteg.h src/scanWorker.h
	gcc -c $(CFLAGS) -o $@ src/csteg.c

# Don't use -c here, since need to link to create finished product
csteg.bin: src/*
	gcc $(CFLAGS) -o $@ bin/*.o

clean:
	rm -f *.bin
	rm -f bin/*.o
