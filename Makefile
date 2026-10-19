# IBM 1401 emulator and Autocoder for VM/370plus - PC build
CC     = cc
CFLAGS = -std=c89 -pedantic -Wall -Wextra -O2

all: ibm1401 autocoder

ibm1401: src/ibm1401.c src/cpu.c src/io.c src/bcd.c src/i1401.h src/bcd.h
	$(CC) $(CFLAGS) -o $@ src/ibm1401.c src/cpu.c src/io.c src/bcd.c

autocoder: src/autocoder.c src/bcd.c src/bcd.h
	$(CC) $(CFLAGS) -o $@ src/autocoder.c src/bcd.c

test: all
	tests/run_tests.sh

clean:
	rm -f ibm1401 autocoder
