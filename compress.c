/*
	exhal / inhal (de)compression routines

	This code is released under the terms of the MIT license.
	See COPYING.txt for details.
	
	Copyright (c) 2013-2018 Devin Acker

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
	
*/

#include <stdio.h>
#include <string.h>
#include "compress.h"
#include "uthash.h"

#ifdef DEBUG_OUT
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

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

// used to hash and index byte tuples
typedef struct {
	int      bytes;
	uint16_t offset;
	UT_hash_handle hh;
} tuple_t;
// turn 4 bytes into a single integer for quicker hashing/searching
#define COMBINE(w, x, y, z) ((w << 24) | (x << 16) | (y << 8) | z)

typedef struct {
	uint8_t *unpacked;
	size_t inputsize;
	uint8_t *packed;
	
	// current input/output positions
	uint32_t  inpos;
	uint32_t  outpos;

	// used to collect data which should be written uncompressed
	uint8_t  dontpack[LONG_RUN_SIZE];
	uint16_t dontpacksize;

	// index of first locations of byte-tuples used to speed up LZ string search
	tuple_t *offsets;
	
} pack_context_t;

// ------------------------------------------------------------------------------------------------
static pack_context_t* pack_context_alloc(uint8_t *unpacked, size_t inputsize, uint8_t *packed) {
	pack_context_t *this;
	
	if (inputsize > DATA_SIZE) return 0;
	if (!(this = calloc(1, sizeof(*this)))) return 0;
	
	this->unpacked  = unpacked;
	this->inputsize = inputsize;
	this->packed    = packed;
	
	// index locations of all 4-byte sequences occurring in the input
	for (uint16_t i = 0; inputsize >= 4 && i < inputsize - 4; i++) {
		tuple_t *tuple;
		int currbytes = COMBINE(unpacked[i], unpacked[i+1], unpacked[i+2], unpacked[i+3]);
		
		// has this one been indexed already
		HASH_FIND_INT(this->offsets, &currbytes, tuple);
		if (!tuple) {
			tuple = (tuple_t*)malloc(sizeof(tuple_t));
			tuple->bytes = currbytes;
			tuple->offset = i;
			HASH_ADD_INT(this->offsets, bytes, tuple);
		}
	}
	
	return this;
}

// ------------------------------------------------------------------------------------------------
static void pack_context_free(pack_context_t* this) {
	tuple_t *curr, *temp;
	HASH_ITER(hh, this->offsets, curr, temp) {
		HASH_DEL(this->offsets, curr);
		free(curr);
	}
	
	free(this);
}

// ------------------------------------------------------------------------------------------------
static inline size_t input_bytes_left(const pack_context_t* this) {
	return this->inputsize - this->inpos;
}

// ------------------------------------------------------------------------------------------------
// Reverses the order of bits in a byte.
// One of the back reference methods does this. As far as game data goes, it seems to be
// pretty useful for compressing graphics.
static inline uint8_t rotate (uint8_t i) {
	uint8_t j = 0;
	if (i & 0x01) j |= 0x80;
	if (i & 0x02) j |= 0x40;
	if (i & 0x04) j |= 0x20;
	if (i & 0x08) j |= 0x10;
	if (i & 0x10) j |= 0x08;
	if (i & 0x20) j |= 0x04;
	if (i & 0x40) j |= 0x02;
	if (i & 0x80) j |= 0x01;
	
	return j;
}

// ------------------------------------------------------------------------------------------------
static inline void rle_candidate(rle_t *candidate, size_t size, uint16_t data, method_e method) {
	// if this is better than the current candidate, use it
	if (method == rle_16 && size >= 2*LONG_RUN_SIZE)
		size = 2*LONG_RUN_SIZE;
	else if (size > LONG_RUN_SIZE)
		size = LONG_RUN_SIZE;
	
	if (size > 2 && size > candidate->size) {
		candidate->size = size;
		candidate->data = data;
		candidate->method = method;
		
		debug("\trle_check: found new candidate (size = %d, method = %d)\n", size, method);
	}
}

// ------------------------------------------------------------------------------------------------
// Searches for possible RLE compressed data.
// start and current are positions within the uncompressed input stream.
// fast enables faster compression by ignoring sequence RLE.
static void rle_check(const pack_context_t *this, rle_t *candidate, int fast) {
	uint8_t *start   = this->unpacked;
	uint8_t *current = start + this->inpos;
	size_t insize = this->inputsize;
	size_t size;
	
	candidate->size = 0;
	candidate->data = 0;
	candidate->method = 0;
	
	// check for possible 8-bit RLE
	for (size = 0; size <= LONG_RUN_SIZE && current + size < start + insize; size++) {
		if (current[size] != current[0]) break;
	}
	rle_candidate(candidate, size, current[0], rle_8);

	// check for possible 16-bit RLE
	uint16_t first = current[0] | (current[1] << 8);
	for (size = 0; size <= 2*LONG_RUN_SIZE && current + size < start + insize - 1; size += 2) {
		uint16_t next = current[size] | (current[size + 1] << 8);
		if (next != first) break;
	}
	rle_candidate(candidate, size, first, rle_16);
	
	// fast mode: don't use sequence RLE
	if (fast) return;
	
	// check for possible sequence RLE
	for (size = 0; size <= LONG_RUN_SIZE && current + size < start + insize; size++) {
		if (current[size] != (current[0] + size)) break;
	}
	rle_candidate(candidate, size, current[0], rle_seq);	
}

// ------------------------------------------------------------------------------------------------
static inline void backref_candidate(backref_t *candidate, size_t offset, size_t size, method_e method) {			
	// if this is better than the current candidate, use it
	if (size > LONG_RUN_SIZE) size = LONG_RUN_SIZE;
	if (size >= 4 && size > candidate->size) {
		candidate->size = size;
		candidate->offset = offset;
		candidate->method = method;
		
		debug("\tref_search: found new candidate (offset: %4x, size: %d, method = %d)\n", offset, size, method);
	}
}

// ------------------------------------------------------------------------------------------------
// Searches for the best possible back reference.
// start and current are positions within the uncompressed input stream.
// fast enables fast mode which only uses regular forward references
static void ref_search (const pack_context_t *this, backref_t *candidate, int fast) {
	uint8_t *start   = this->unpacked;
	uint8_t *current = start + this->inpos;
	size_t insize = this->inputsize;
	tuple_t *offsets = this->offsets;
	
	uint16_t size;
	int currbytes;
	tuple_t *tuple;
	
	candidate->size = 0;
	candidate->offset = 0;
	candidate->method = 0;
	
	// references to previous data which goes in the same direction
	// see if this byte sequence exists elsewhere, then start searching.
	currbytes = COMBINE(current[0], current[1], current[2], current[3]);
	HASH_FIND_INT(offsets, &currbytes, tuple);
	if (tuple) for (uint8_t *pos = start + tuple->offset; pos < current; pos++) {
		// see how many bytes in a row are the same between the current uncompressed data
		// and the data at the position being searched
		for (size = 0; size <= LONG_RUN_SIZE && current + size < start + insize; size++) {
			if (pos[size] != current[size]) break;
		}
		backref_candidate(candidate, pos - start, size, lz_norm);
	}
	
	// fast mode: forward references only
	if (fast) return;
	
	// references to data where the bits are rotated
	currbytes = COMBINE(rotate(current[0]), rotate(current[1]), rotate(current[2]), rotate(current[3]));
	HASH_FIND_INT(offsets, &currbytes, tuple);
	if (tuple) for (uint8_t *pos = start + tuple->offset; pos < current; pos++) {	
		// now repeat the check with the bit rotation method
		for (size = 0; size <= LONG_RUN_SIZE && current + size < start + insize; size++) {
			if (pos[size] != rotate(current[size])) break;
		}
		backref_candidate(candidate, pos - start, size, lz_rot);
	}
	
	// references to data which goes backwards
	currbytes = COMBINE(current[3], current[2], current[1], current[0]);
	HASH_FIND_INT(offsets, &currbytes, tuple);
	// add 3 to offset since we're starting at the end of the 4 byte sequence here
	if (tuple) for (uint8_t *pos = start + tuple->offset + 3; pos < current; pos++) {
		// now repeat the check but go backwards
		for (size = 0; size <= LONG_RUN_SIZE && start + size <= pos
		     && current + size < start + insize; size++) {
			if (start[pos - start - size] != current[size]) break;
		}
		backref_candidate(candidate, pos - start, size, lz_rev);
	}
}

// ------------------------------------------------------------------------------------------------
static inline int write_check_size(const pack_context_t *this, size_t size) {
	return this->outpos + this->dontpacksize + size < DATA_SIZE;
}

// ------------------------------------------------------------------------------------------------
// Write uncompressed data to the output stream.
// Returns number of bytes written.
static uint16_t write_raw (pack_context_t *this) {
	uint8_t *out = this->packed;
	uint16_t insize = this->dontpacksize;

	if (!insize) return 0;

	debug("%04x %04x write_raw: writing %d bytes unpacked data\n", 
		this->inpos - insize, this->outpos, insize);

	uint16_t size = insize - 1;
	int outsize;
	
	if (size >= RUN_SIZE) {
		// write_check_size already accounts for size of raw data,
		// but also check the size of the command/size byte(s)
		outsize = 2;
		if (!write_check_size(this, outsize)) return 0;
		
		// write command byte + MSB of size
		out[this->outpos++] = 0xE0 + (size >> 8);
		// write LSB of size
		out[this->outpos++] = size & 0xFF;
	}
	// normal size run
	else {
		outsize = 1;
		if (!write_check_size(this, outsize)) return 0;
		
		// write command byte / size
		out[this->outpos++] = size;
	}
	
	// write data
	memcpy(&out[this->outpos], this->dontpack, insize);
	this->outpos += insize;
	this->dontpacksize = 0;
	// total size written is the command + size + all data
	return outsize + insize;
}

// ------------------------------------------------------------------------------------------------
static inline uint16_t backref_outsize(const backref_t *backref) {
	return (backref->size - 1 >= RUN_SIZE) ? 4 : 3;
}

// ------------------------------------------------------------------------------------------------
// Writes a back reference to the compressed output stream.
// Returns number of bytes written
static uint16_t write_backref (pack_context_t *this, const backref_t *backref) {
	uint16_t size = backref->size - 1;
	uint8_t *out = this->packed;
	
	uint16_t outsize = backref_outsize(backref);
	if (!write_check_size(this, outsize)) return 0;
	
	// flush the raw data buffer first
	write_raw(this);
	
	debug("%04x %04x write_backref: writing backref to %4x, size %d (method %d)\n", 
		this->inpos, this->outpos, backref->offset, backref->size, backref->method);
	
	// long run
	if (size >= RUN_SIZE) {
		// write command byte / MSB of size
		out[this->outpos++] = (0xF0 + (backref->method << 2)) | (size >> 8);
		// write LSB of size
		out[this->outpos++] = size & 0xFF;
	} 
	// normal size run
	else {		
		// write command byte / size
		out[this->outpos++] = (0x80 + (backref->method << 5)) | size;
	}
	
	// write MSB of offset
	out[this->outpos++] = backref->offset >> 8;
	// write LSB of offset
	out[this->outpos++] = backref->offset & 0xFF;
	
	this->inpos += backref->size;
	return outsize;
}

// ------------------------------------------------------------------------------------------------
static inline uint16_t rle_outsize(const rle_t *rle) {
	uint16_t size = (rle->size - 1 >= RUN_SIZE) ? 3 : 2;
	if (rle->method == rle_16) size++; // account for extra byte of value
	return size;
}

// ------------------------------------------------------------------------------------------------
// Writes RLE data to the compressed output stream.
// Returns number of bytes written
static uint16_t write_rle (pack_context_t *this, const rle_t *rle) {
	uint16_t size;
	uint8_t *out = this->packed;
	
	uint16_t outsize = rle_outsize(rle);
	if (!write_check_size(this, outsize)) return 0;
	
	if (rle->method == rle_16) {
		size = (rle->size / 2) - 1;
	} else {
		size = rle->size - 1;
	}
	
	// flush the raw data buffer first
	write_raw(this);
	
	debug("%04x %04x write_rle: writing %d bytes of data 0x%02x (method %d)\n", 
		this->inpos, this->outpos, rle->size, rle->data, rle->method);
	
	// long run
	if (size >= RUN_SIZE) {
		// write command byte / MSB of size
		out[this->outpos++] = (0xE4 + (rle->method << 2)) | (size >> 8);
		// write LSB of size
		out[this->outpos++] = size & 0xFF;
	}
	// normal size run
	else {
		// write command byte / size
		out[this->outpos++] = (0x20 + (rle->method << 5)) | size;
	}
	
	out[this->outpos++] = rle->data;
	// write upper byte of 16-bit RLE (and adjust written data size)
	if (rle->method == rle_16) {
		out[this->outpos++] = rle->data >> 8;
	}
	
	this->inpos += rle->size;
	return outsize;
}

// ------------------------------------------------------------------------------------------------
// Writes a single byte of raw (literal) data from the input.
// Returns number of bytes written
static uint16_t write_next_byte(pack_context_t *this) {
	if (!write_check_size(this, 1)) return 0;
	
	this->dontpack[this->dontpacksize++] = this->unpacked[this->inpos++];
	
	// if the raw data buffer is full, flush it
	if (this->dontpacksize == LONG_RUN_SIZE) {
		write_raw(this);
	}
	
	return 1;
}

// ------------------------------------------------------------------------------------------------
// Writes a single byte to terminate the compressed data.
// Returns number of bytes written
static uint16_t write_trailer(pack_context_t *this) {
	if (!write_check_size(this, 1)) return 0;
	
	write_raw(this);
	
	//add the terminating byte
	this->packed[this->outpos++] = 0xFF;
	
	return 1;
}

// ------------------------------------------------------------------------------------------------
static void pack_normal(pack_context_t *this, int fast) {
	size_t inputsize = this->inputsize;
	// backref and RLE compression candidates
	backref_t backref = {0};
	rle_t     rle = {0};
	
	while (inputsize > 0) {
		// check for a potential RLE
		rle_check(this, &rle, fast);
		// check for a potential back reference
		if (rle.size < LONG_RUN_SIZE && inputsize >= 4)
			ref_search(this, &backref, fast);
		else backref.size = 0;
		
		// if the backref is a better candidate, use it
		if (backref.size > rle.size) {
			if (!write_backref(this, &backref)) break;
		}
		// or if the RLE is a better candidate, use it instead
		else if (rle.size >= 2) {
			if (!write_rle(this, &rle)) break;
		}
		// otherwise, write this byte uncompressed
		else {
			if (!write_next_byte(this)) break;
		}
		
		inputsize = input_bytes_left(this);
	}
}

// ------------------------------------------------------------------------------------------------
static void pack_optimal(pack_context_t *this, int fast) {
	size_t inputsize = this->inputsize;
	// backref and RLE compression candidates
	backref_t backref = {0};
	rle_t     rle = {0};
	
	// test - just go through entire input and score each byte
	typedef struct node_s {
		// previous and next nodes in directed graph
		// (populated when doing shortest-path search)
		struct node_s *next, *prev;
		// distance to second neighboring node (first is n+1)
		size_t neighbor;
		// graph edge length between this and neighbor (i.e. size of compressed data)
		size_t length;
		// distance to start of data
		size_t distance;
		// backref used for compression (else RLE if neighbor > 0)
		int backref;
		// RLE data or backref offset
		uint16_t data;
		// RLE/backref method used
		method_e method;
	} node_t;
	node_t *nodes = calloc(inputsize+1, sizeof(node_t));
	
	for (this->inpos = 0; this->inpos < inputsize; this->inpos++) {
		node_t *node = nodes+this->inpos;
		node->distance = 1<<16;
		node->next = node->prev = 0;

		// check for a potential RLE
		rle_check(this, &rle, fast);
		// check for a potential back reference
		if (rle.size < LONG_RUN_SIZE && inputsize >= 4)
			ref_search(this, &backref, fast);
		else backref.size = 0;
		
		// if the backref is a better candidate, use it
		if (backref.size > rle.size) {
			node->neighbor = backref.size;
			node->length   = backref_outsize(&backref);
			node->method   = backref.method;
			node->data     = backref.offset;
			node->backref  = 1;
		}
		// or if the RLE is a better candidate, use it instead
		else if (rle.size >= 2) {
			node->neighbor = rle.size;
			node->length   = rle_outsize(&rle);
			node->method   = rle.method;
			node->data     = rle.data;
		}
	}
	
	// find shortest path through input
	nodes[0].distance = 0;
	nodes[inputsize].distance = 1<<16;
	
	node_t *node, *other;
	
	for (size_t i = 0; i < inputsize; i++) {
		node = nodes+i;
		size_t newdist;
		
		// check first neighbor (next byte)
		other = node+1;
		newdist = node->distance + 2; // at least 1 literal byte + 1 control byte
		if (newdist < other->distance) {
			other->distance = newdist;
			other->prev = node;
		}
		
		// check second neighbor (next byte after compression, if possible)
		if (!node->neighbor) continue;
		
		other = node+node->neighbor;
		newdist = node->distance + node->length;
		if (newdist < other->distance) {
			other->distance = newdist;
			other->prev = node;
		}
	}
	debug("final distance = %u prev = %04x\n", nodes[inputsize].distance, nodes[inputsize].prev);
	// create path back from end to start of data
	for (node = nodes+inputsize; node->prev; node = node->prev) {
		debug("node = %u prev = %u\n", node-nodes, node->prev-nodes);
		node->prev->next = node;
	}
	
	// compress data based on shortest path
	this->inpos = 0;
	for (node = nodes; node->next; node = node->next) {
		debug("node = %u next = %u\n", node-nodes, node->next-nodes);
		if (node->next == node+1) {
			if (!write_next_byte(this)) break;
		} else if (node->backref) {
			backref.size   = node->neighbor;
			backref.method = node->method;
			backref.offset = node->data;
			if (!write_backref(this, &backref)) break;
		} else {
			rle.size   = node->neighbor;
			rle.method = node->method;
			rle.data   = node->data;
			if (!write_rle(this, &rle)) break;
		}
	}
	
	free(nodes);
}

// ------------------------------------------------------------------------------------------------
// Compresses a file of up to 64 kb.
// unpacked/packed are 65536 byte buffers to read/from write to, 
// inputsize is the length of the uncompressed data.
// Returns the size of the compressed data in bytes, or 0 if compression failed.
size_t exhal_pack2(uint8_t *unpacked, size_t inputsize, uint8_t *packed, const pack_options_t *options) {
	size_t outpos = 0;
	
	debug("inputsize = %d\n", inputsize);
	
	pack_context_t *ctx = pack_context_alloc(unpacked, inputsize, packed);
	if (!ctx) return 0;

	if (inputsize > 0) {
		if (options && options->optimal)
			pack_optimal(ctx, options ? options->fast : 0);
		else
			pack_normal(ctx, options ? options->fast : 0);
	}
		
	if (write_trailer(ctx)) {
		// compressed data was written successfully
		outpos = (size_t)ctx->outpos;
	}

	pack_context_free(ctx);
	return outpos;
}

// ------------------------------------------------------------------------------------------------
size_t exhal_pack(uint8_t *unpacked, size_t inputsize, uint8_t *packed, int fast) {
	pack_options_t options = {
		.fast = fast,
	};
	return exhal_pack2(unpacked, inputsize, packed, &options);
}

// ------------------------------------------------------------------------------------------------
// Decompresses a file of up to 64 kb.
// unpacked/packed are 65536 byte buffers to read/from write to, 
// Returns the size of the uncompressed data in bytes or 0 if decompression failed.
size_t exhal_unpack(uint8_t *packed, uint8_t *unpacked, unpack_stats_t *stats) {
	// current input/output positions
	uint32_t  inpos = 0;
	uint32_t  outpos = 0;

	uint8_t  input;
	uint16_t command, length, offset;
	
	if (stats) memset(stats, 0, sizeof(*stats));
	
	while (1) {
		int32_t insize = DATA_SIZE - inpos;
		
		// read command byte from input
		if (insize < 1) return 0;
		input = packed[inpos++];
		
		// command 0xff = end of data
		if (input == 0xFF)
			break;
		
		// check if it is a long or regular command, get the command no. and size
		if ((input & 0xE0) == 0xE0) {
			if (insize < 1) return 0;
			
			command = (input >> 2) & 0x07;
			// get LSB of length from next byte
			length = (((input & 0x03) << 8) | packed[inpos++]) + 1;
		} else {
			command = input >> 5;
			length = (input & 0x1F) + 1;
		}
		
		// don't try to decompress > 64kb
		if (((command == 2) && (outpos + 2*length > DATA_SIZE))
			 || (outpos + length > DATA_SIZE)) {
			return 0;
		}
		
		switch (command) {
		// write uncompressed bytes
		case 0:
			if (insize < length) return 0;
			debug("%06x: writing %u raw bytes\n", inpos, length);
			memcpy(&unpacked[outpos], &packed[inpos], length);
			
			outpos += length;
			inpos  += length;
			break;
		
		// 8-bit RLE
		case 1:
			if (insize < 1) return 0;
			debug("%06x: writing %u bytes RLE, value %02x\n", inpos, length, packed[inpos]);
			for (int i = 0; i < length; i++)
				unpacked[outpos++] = packed[inpos];

			inpos++;
			break;

		// 16-bit RLE
		case 2:
			if (insize < 2) return 0;
			debug("%06x: writing %u words RLE, value %02x%02x\n", inpos, length, packed[inpos], packed[inpos+1]);
			for (int i = 0; i < length; i++) {
				unpacked[outpos++] = packed[inpos];
				unpacked[outpos++] = packed[inpos+1];
			}

			inpos += 2;
			break;

		// 8-bit increasing sequence
		case 3:
			if (insize < 1) return 0;
			debug("%06x: writing %u bytes sequence RLE, value %02x\n", inpos, length, packed[inpos]);
			for (int i = 0; i < length; i++)
				unpacked[outpos++] = packed[inpos] + i;

			inpos++;
			break;
			
		// regular backref
		// (offset is big-endian)
		case 4:
		case 7:
			// 7 isn't a real method number, but it behaves the same as 4 due to a quirk in how
			// the original decompression routine is programmed. (one of Parasyte's docs confirms
			// this for GB games as well). let's handle it anyway
			command = 4;

			if (insize < 2) return 0;
			
			offset = (packed[inpos] << 8) | packed[inpos+1];
			debug("%06x: writing %u byte forward ref to %x\n", inpos, length, offset);
			
			if (offset + length > DATA_SIZE) return 0;
			
			for (int i = 0; i < length; i++)
				unpacked[outpos++] = unpacked[offset + i];

			inpos += 2;
			break;

		// backref with bit rotation
		// (offset is big-endian)
		case 5:
			if (insize < 2) return 0;
			
			offset = (packed[inpos] << 8) | packed[inpos+1];
			debug("%06x: writing %u byte rotated ref to %x\n", inpos, length, offset);
			
			if (offset + length > DATA_SIZE) return 0;
			
			for (int i = 0; i < length; i++)
				unpacked[outpos++] = rotate(unpacked[offset + i]);

			inpos += 2;
			break;

		// backwards backref
		// (offset is big-endian)
		case 6:
			if (insize < 2) return 0;
			
			offset = (packed[inpos] << 8) | packed[inpos+1];
			debug("%06x: writing %u byte backward ref to %x\n", inpos, length, offset);
			
			if (offset < length - 1) return 0;
			
			for (int i = 0; i < length; i++)
				unpacked[outpos++] = unpacked[offset - i];

			inpos += 2;
		}
		
		// keep track of how many times each compression method is used
		if (stats) stats->methoduse[command]++;
	}

	if (stats) stats->inputsize = (size_t)inpos;

	return (size_t)outpos;
}

// ------------------------------------------------------------------------------------------------
// Decompress data from an offset into a file
size_t exhal_unpack_from_file(FILE *file, size_t offset, uint8_t *unpacked, unpack_stats_t *stats) {
	uint8_t packed[DATA_SIZE] = {0};
	
	fseek(file, offset, SEEK_SET);
	fread((void*)packed, DATA_SIZE, 1, file);
	if (!ferror(file))
		return exhal_unpack(packed, unpacked, stats);
		
	return 0;
}
