all:
	gcc -DTESTING src/queue.c -o bin/queue.o -c
	gcc -DTESTING src/trie.c -o bin/trie.o -c
	#gcc -DTESTING -o trie.bin bin/trie.o  bin/queue.o
	gcc -DTESTING src/scanWorker.c -g -o bin/scanWorker.o -c
	gcc -DTESTING src/csteg.c -g -o bin/csteg.o -c
	gcc -DTESTING -g -o prototype.bin bin/*.o 

	
