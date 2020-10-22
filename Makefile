all: xcheck

xcheck: xcheck.o fileio.o
	gcc -o xcheck xcheck.o fileio.o

xcheck.o: src/xcheck.c src/fileio.c
	gcc -c src/xcheck.c src/fileio.c -Wall -g

clean:
	rm -rf *.o
	rm xcheck
