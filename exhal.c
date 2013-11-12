/*
	exhal - HAL Laboratory decompression tool
	by Devin Acker

	Usage:
	exhal romfile offset outfile

	This code is released under the terms of the MIT license.
	See COPYING.txt for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include "compress.h"

int main (int argc, char **argv) {
	printf("exhal - %s %s\nby Devin Acker (Revenant)\n\n", __DATE__, __TIME__);
	
	if (argc != 4) {
		printf("Usage:\n%s romfile offset outfile\n", argv[0]);
		printf("Example: %s kirbybowl.sfc 0x70000 test.bin\n\n", argv[0]);
		printf("offset can be in either decimal or hex.\n");
		exit(-1);
	}
	
	FILE   *infile, *outfile;
	
	// open ROM file for input
	infile = fopen(argv[1], "rb");
	if (!infile) {
		printf("Error: unable to open %s\n", argv[1]);
		exit(-1);
	}
	
	// open target file for output
	outfile = fopen(argv[3], "wb");
	if (!outfile) {
		printf("Error: unable to open %s\n", argv[1]);
		exit(-1);
	}
	
	size_t   outputsize;
	uint32_t fileoffset;
	uint8_t  unpacked[DATA_SIZE];
	
	fileoffset = strtol(argv[2], NULL, 0);
	
	// decompress the file
	outputsize = unpack_from_file(infile, fileoffset, unpacked);
	
	// write the uncompressed data to the file
	fseek(outfile, 0, SEEK_SET);
	fwrite((const void*)unpacked, 1, outputsize, outfile);
	
	printf("Uncompressed size: %zd bytes\n", outputsize);
	
	fclose(infile);
	fclose(outfile);
}
