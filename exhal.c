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
	printf("exhal - " __DATE__ " " __TIME__"\nby Devin Acker (Revenant)\n\n");
	
	if (argc != 4) {
		fprintf(stderr, "Usage:\n%s romfile offset outfile\n"
		                "Example: %s kirbybowl.sfc 0x70000 test.bin\n\n"
		                "offset can be in either decimal or hex.\n",
		                argv[0], argv[0]);
		exit(-1);
	}
	
	FILE   *infile, *outfile;
	
	// open ROM file for input
	infile = fopen(argv[1], "rb");
	if (!infile) {
		fprintf(stderr, "Error: unable to open %s\n", argv[1]);
		exit(-1);
	}
	
	// open target file for output
	outfile = fopen(argv[3], "wb");
	if (!outfile) {
		fprintf(stderr, "Error: unable to open %s\n", argv[3]);
		exit(-1);
	}
	
	size_t   outputsize, fileoffset;
	uint8_t  unpacked[DATA_SIZE] = {0};
	
	fileoffset = strtol(argv[2], NULL, 0);
	
	// decompress the file
	fseek(infile, 0, SEEK_END);
	if (fileoffset < ftell(infile)) {
		outputsize = unpack_from_file(infile, fileoffset, unpacked);
	} else {
		fprintf(stderr, "Error: Unable to decompress %s because an invalid offset was specified\n"
		                "       (must be between zero and 0x%X).\n", argv[1], ftell(infile));
		exit(-1);
	}
	
	if (outputsize) {
		// write the uncompressed data to the file
		fseek(outfile, 0, SEEK_SET);
		fwrite((const void*)unpacked, 1, outputsize, outfile);
		if (ferror(outfile)) {
			perror("Error writing output file");
			exit(-1);
		}
		
		printf("Uncompressed size: %zu bytes\n", outputsize);
	} else {
		fprintf(stderr, "Error: Unable to decompress %s because the output would have been larger than\n"
		                "       64 kb. The input at 0x%X is likely not valid compressed data.\n", argv[1], fileoffset);
	}
	
	fclose(infile);
	fclose(outfile);
}
