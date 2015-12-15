# HAL (de)compression tools
# copyright 2013 Devin Acker (Revenant)
# See copying.txt for legal information.

CFLAGS  += -std=c99 -Os -Wall -s -fpic

# Add extension when compiling for Windows
ifdef SystemRoot
	CC  = gcc 
	EXT = .exe
	RM  = del
endif

# Comment this line to suppress detailed decompression information on stdout
#DEFINES += -DEXTRA_OUT
# Uncomment this line to enable debug output
#DEFINES += -DDEBUG_OUT

all: inhal$(EXT) exhal$(EXT)

clean:
	$(RM) inhal$(EXT) exhal$(EXT) compress.o libexhal.so
	
inhal$(EXT): inhal.c compress.o
	$(CC) $(DEFINES) $(CFLAGS) -o inhal$(EXT) inhal.c compress.o
	
exhal$(EXT): exhal.c compress.o
	$(CC) $(DEFINES) $(CFLAGS) -o exhal$(EXT) exhal.c compress.o
	
compress.o: compress.c
	$(CC) $(DEFINES) $(CFLAGS) -c compress.c

libexhal.so: compress.o
	gcc -shared -o $@ $^

install: all libexhal.so
	mkdir -pv /usr/local/bin
	cp inhal$(EXT) exhal$(EXT) /usr/local/bin
	mkdir -pv /usr/local/lib
	cp libexhal.so /usr/local/lib
	mkdir -pv /usr/local/include
	cp include/* /usr/local/include
