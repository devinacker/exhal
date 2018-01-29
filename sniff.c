
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "compress.h"

int main (int argc, char **argv) {
	printf("sniff - " __DATE__ " " __TIME__"\nby Devin Acker (Revenant)\n\n");
	
	if (argc != 2) {
		fprintf(stderr, "Usage:\n%s romfile offset outfile\n"
		                "Example: %s kirbybowl.sfc\n",
		                argv[0], argv[0]);
		exit(-1);
	}
	
	FILE   *infile;
	
	// open ROM file for input
	infile = fopen(argv[1], "rb");
	if (!infile) {
		fprintf(stderr, "Error: unable to open %s\n", argv[1]);
		exit(-1);
	}
	
	size_t   outputsize, filesize;
	uint8_t  unpacked[DATA_SIZE] = {0};
	unpack_stats_t stats;
	
	// decompress the file
	fseek(infile, 0, SEEK_END);
	filesize = ftell(infile);
	
	for (int i = 0; i < filesize; i++) {
		fseek(infile, i, SEEK_SET);
		outputsize = exhal_unpack_from_file(infile, i, unpacked, &stats);
		
		if (outputsize > stats.inputsize
			&& outputsize >= 1024 /* TODO set minimum sizes/ratio/etc */) {
			printf("%06x: %u -> %u bytes\n", i, (unsigned)stats.inputsize, (unsigned)outputsize);
		}
	}
	
	fclose(infile);
}
