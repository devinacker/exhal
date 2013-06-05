# HAL (de)compression tools
# copyright 2013 Devin Acker (Revenant)
# See copying.txt for legal information.

CC      = gcc
FLAGS   = -std=c99 -O2 -Wall

# Add extension when compiling for Windows
ifdef SystemRoot
	EXT = .exe
endif

# Comment this line to suppress detailed decompression information on stdout
DEFINES += -DEXTRA_OUT
# Uncomment this line to enable debug output
#DEFINES += -DDEBUG_OUT

all: inhal$(EXT) exhal$(EXT)

clean:
	del inhal$(EXT) exhal$(EXT) compress.o
	
inhal$(EXT): inhal.c compress.o
	$(CC) $(DEFINES) $(FLAGS) -o inhal$(EXT) inhal.c compress.o
	
exhal$(EXT): exhal.c compress.o
	$(CC) $(DEFINES) $(FLAGS) -o exhal$(EXT) exhal.c compress.o
	
compress.o: compress.c
	$(CC) $(DEFINES) $(FLAGS) -c compress.c