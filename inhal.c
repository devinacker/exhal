/*
	inhal - HAL Laboratory compression tool
	by Devin Acker
   
	Usage:
	inhal infile romfile offset
	inhal -n infile outfile
   
	This code is released under the terms of the MIT license.
	See COPYING.txt for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "compress.h"

int main (int argc, char **argv) {
	printf("inhal - " __DATE__ " " __TIME__"\nby Devin Acker (Revenant)\n\n");
	
	if (argc < 4) {
		printf("To insert compressed data into a ROM:\n");
		printf("%s [-fast] infile romfile offset\n", argv[0]);
		
		printf("To write compressed data to a new file:\n");
		printf("%s [-fast] -n infile outfile\n\n", argv[0]);
		
		printf("Running with the -fast switch increases compression speed at the expense of size.\n");
		
		printf("\nExample:\n%s -fast test.chr kirbybowl.sfc 0x70000\n", argv[0]);
		printf("%s -n test.chr test-packed.bin\n\n", argv[0]);
		printf("offset can be in either decimal or hex.\n");
		exit(-1);
	}
	
	FILE   *infile, *outfile;
	int    fileoffset;
	int    newfile = 0;
	int    fast    = 0;
	
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n"))
			newfile = 1;
		else if (!strcmp(argv[i], "-fast")) 
			fast = 1;
	}
	
	if (fast)
		printf("Fast compression enabled.\n");
		
	// check for -n switch
	if (newfile) {
		fileoffset = 0;
		infile = fopen(argv[argc - 2], "rb");
		outfile = fopen(argv[argc - 1], "wb");
	} else {
		fileoffset = strtol(argv[argc - 1], NULL, 0);
		infile = fopen(argv[argc - 3], "rb");
		outfile = fopen(argv[argc - 2], "r+b");
	}
	
	if (!infile) {
		printf("Error: unable to open input file\n");
		exit(-1);
	}
	if (!outfile) {
		printf("Error: unable to open output file\n");
		exit(-1);
	}
	
	size_t   inputsize, outputsize;
	uint8_t  unpacked[DATA_SIZE];
	uint8_t  packed[DATA_SIZE] = {0};
	
	// check size of input file
	fseek(infile, 0, SEEK_END);
	inputsize = ftell(infile);
	
	printf("Uncompressed size: %zd bytes\n", inputsize);
	
	if (inputsize > DATA_SIZE) {
		printf("Error: File must be a maximum of 65,536 bytes!\n");
		exit(-1);
	} else if (!inputsize) {
		printf("Error: Input file is empty!\n");
		exit(-1);
	}
	
	// read the file
	fseek(infile, 0, SEEK_SET);
	fread(unpacked, sizeof(uint8_t), inputsize, infile);
	if (ferror(infile)) {
		perror("Error reading input file");
		exit(-1);
	}
	
	// compress the file
	clock_t time = clock();
	outputsize = pack(unpacked, inputsize, packed, fast);
	time = clock() - time;

	if (outputsize) {
		// write the compressed data to the file
		fseek(outfile, fileoffset, SEEK_SET);
		fwrite((const void*)packed, 1, outputsize, outfile);
		if (ferror(outfile)) {
			perror("Error writing output file");
			exit(-1);
		}
		
		printf("Compressed size:   % zu bytes\n", outputsize);
		printf("Compression ratio:  %4.2f%%\n", 100 * (double)outputsize / inputsize);
		printf("Compression time:   %4.3f seconds\n\n", (double)time / CLOCKS_PER_SEC);
		
		printf("Inserted at 0x%06X - 0x%06lX\n", fileoffset, ftell(outfile) - 1);
	} else {
		printf("Error: File could not be compressed because the resulting compressed data would\n"
		       "       have been larger than 64 kb.\n");
	}
	
	fclose(infile);
	fclose(outfile);
}
