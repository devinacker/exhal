# HAL (de)compression tools
# copyright 2013 Devin Acker (Revenant)
# See copying.txt for legal information.

CFLAGS  += -std=c99 -O3 -Wall -s

# Add extension when compiling for Windows
ifeq ($(OS), Windows_NT)
	CC  = gcc 
	EXT = .exe
endif

# Comment this line to suppress detailed decompression information on stdout
DEFINES += -DEXTRA_OUT
# Uncomment this line to enable debug output
#DEFINES += -DDEBUG_OUT

CFLAGS += $(DEFINES)

all: inhal$(EXT) exhal$(EXT) sniff$(EXT)

clean:
	$(RM) inhal$(EXT) exhal$(EXT) sniff$(EXT) *.o

sniff$(EXT): sniff.o compress.o memmem.o
	$(CC) $(CFLAGS) -o $@ $^
	
inhal$(EXT): inhal.o compress.o memmem.o
	$(CC) $(CFLAGS) -o $@ $^
	
exhal$(EXT): exhal.o compress.o memmem.o
	$(CC) $(CFLAGS) -o $@ $^
