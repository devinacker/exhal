/*
	inhal - HAL Laboratory compression tool
	by Devin Acker
   
	Usage:
	inhal infile romfile offset
	inhal -n infile outfile
   
	This code is released under the terms of the MIT license.
	See copying.txt for details.
*/

#include <stdio.h>
#include <time.h>
#include "compress.h"

int main (int argc, char **argv) {
	printf("inhal - %s %s\nby Devin Acker (Revenant)\n\n", __DATE__, __TIME__);
	
	if (argc != 4) {
		printf("To insert compressed data into a ROM:\n");
		printf("%s infile romfile offset\n", argv[0]);
		
		printf("To write compressed data to a new file:\n");
		printf("%s -n infile outfile\n", argv[0]);
		
		printf("\nExample:\n%s test.chr kirbybowl.sfc 0x70000\n", argv[0]);
		printf("%s -n test.chr test-packed.bin\n\n", argv[0]);
		printf("offset can be in either decimal or hex.\n");
		exit(-1);
	}
	
	FILE   *infile, *outfile;
	int    fileoffset;
	
	// check for -n switch
	if (!strcmp(argv[1], "-n")) {
		fileoffset = 0;
		infile = fopen(argv[2], "rb");
		outfile = fopen(argv[3], "wb");
	} else {
		fileoffset = strtol(argv[3], NULL, 0);
		infile = fopen(argv[1], "rb");
		outfile = fopen(argv[2], "r+b");
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
	uint8_t  packed[DATA_SIZE];
	memset(packed, 0, DATA_SIZE);
	
	// check size of input file
	fseek(infile, 0, SEEK_END);
	inputsize = ftell(infile);
	
	printf("Uncompressed size: %d bytes\n", inputsize);
	
	if (inputsize > DATA_SIZE) {
		printf("Error: File must be a maximum of 65,536 bytes!\n");
		exit(-1);
	}
	// read the file
	fseek(infile, 0, SEEK_SET);
	fread(unpacked, sizeof(uint8_t), inputsize, infile);
	
	// compress the file
	clock_t time = clock();
	outputsize = pack(unpacked, inputsize, packed);
	time = clock() - time;
	
	// write the compressed data to the file
	fseek(outfile, fileoffset, SEEK_SET);
	fwrite((const void*)packed, 1, outputsize, outfile);
	
	printf("Compressed size: %d bytes\n", outputsize);
	printf("Compression ratio: %4.2f%%\n", 100 * (double)outputsize / inputsize);
	printf("Compression time: %4.3f seconds\n\n", (double)time / CLOCKS_PER_SEC);
	
	printf("Inserted at 0x%06X - 0x%06X\n", fileoffset, ftell(outfile) - 1);
	
	fclose(infile);
	fclose(outfile);
}
