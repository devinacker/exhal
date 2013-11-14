/*
	exhal / inhal (de)compression routines
	by Devin Acker

	This code is released under the terms of the MIT license.
	See COPYING.txt for details.
*/

#ifndef _COMPRESS_H
#define _COMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DATA_SIZE     65536
#define RUN_SIZE      32
#define LONG_RUN_SIZE 1024

size_t pack   (uint8_t *unpacked, size_t inputsize, uint8_t *packed, int fast);
size_t unpack (uint8_t *packed, uint8_t *unpacked);

size_t unpack_from_file (FILE *file, size_t offset, uint8_t *unpacked);

#ifdef __cplusplus
}
#endif

// end include guard
#endif
