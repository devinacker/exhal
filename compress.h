/*
	exhal / inhal (de)compression routines
	by Devin Acker

	This code is released under the terms of the MIT license.
	See copying.txt for details.
*/

#ifndef _COMPRESS_H
#define _COMPRESS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DATA_SIZE     65536
#define RUN_SIZE      32
#define LONG_RUN_SIZE 1024

// compression method values for backref_t and rle_t
typedef enum {
	rle_8   = 0,
	rle_16  = 1,
	rle_seq = 2,

    lz_norm = 0,
	lz_rot  = 1,
	lz_rev  = 2
} method_e;

// used to store and compare backref candidates
typedef struct {
	uint16_t offset, size;
	method_e method;
} backref_t;

// used to store RLE candidates
typedef struct {
	uint16_t size, data;
	method_e method;
} rle_t;

size_t pack   (uint8_t *unpacked, size_t inputsize, uint8_t *packed);
size_t unpack (uint8_t *packed, uint8_t *unpacked);

size_t unpack_from_file (FILE *file, unsigned int offset, uint8_t *unpacked);

#endif
