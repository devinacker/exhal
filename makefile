# HAL (de)compression tools
# copyright 2013 Devin Acker (Revenant)
# See copying.txt for legal information.

CFLAGS  += -std=c99 -Os -Wall -s -fpic
# Add extension when compiling for Windows
ifeq ($(OS), Windows_NT)
	CC  = gcc 
	EXT = .exe
        LIBEXT = .dll
	RM  = del
else
	LIBEXT = .so
endif

# Comment this line to suppress detailed decompression information on stdout
DEFINES += -DEXTRA_OUT
# Uncomment this line to enable debug output
DEFINES += -DDEBUG_OUT

all: inhal$(EXT) exhal$(EXT) libexhal$(LIBEXT) libexhal.a

clean:
	$(RM) inhal$(EXT) exhal$(EXT) compress.o libexhal.so
	
inhal$(EXT): inhal.c compress.o
	$(CC) $(DEFINES) $(CFLAGS) -o inhal$(EXT) inhal.c compress.o
	
exhal$(EXT): exhal.c compress.o
	$(CC) $(DEFINES) $(CFLAGS) -o exhal$(EXT) exhal.c compress.o
	
compress.o: compress.c
	$(CC) $(DEFINES) $(CFLAGS) -c compress.c

libexhal$(LIBEXT): compress.o
	gcc -shared -o $@ $^
libexhal.a: compress.o
	ar rcs $@ $^
install: all libexhal.so
	mkdir -pv /usr/local/bin
	cp inhal$(EXT) exhal$(EXT) /usr/local/bin
	mkdir -pv /usr/local/lib
	cp libexhal$(LIBEXT) /usr/local/lib
	cp libexhal.a /usr/local/lib
	mkdir -pv /usr/local/include
	cp include/* /usr/local/include
