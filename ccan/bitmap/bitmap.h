/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_BITMAP_H_
#define CCAN_BITMAP_H_

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <ccan/endian/endian.h>

typedef unsigned long bitmap_word;

#define BITMAP_WORD_BITS	(sizeof(bitmap_word) * CHAR_BIT)
#define BITMAP_NWORDS(_n)	\
	(((_n) + BITMAP_WORD_BITS - 1) / BITMAP_WORD_BITS)

/*
 * We wrap each word in a structure for type checking.
 */
typedef struct {
	bitmap_word w;
} bitmap;

#define BITMAP_DECLARE(_name, _nbits) \
	bitmap (_name)[BITMAP_NWORDS(_nbits)]

static inline size_t bitmap_sizeof(int nbits)
{
	return BITMAP_NWORDS(nbits) * sizeof(bitmap_word);
}

static inline bitmap *bitmap_alloc(int nbits)
{
	return malloc(bitmap_sizeof(nbits));
}

static inline bitmap_word bitmap_bswap(bitmap_word w)
{
	if (BITMAP_WORD_BITS == 32)
		return cpu_to_be32(w);
	else if (BITMAP_WORD_BITS == 64)
		return cpu_to_be64(w);
}

#define BITMAP_WORD(_bm, _n)	((_bm)[(_n) / BITMAP_WORD_BITS].w)
#define BITMAP_WORDBIT(_n) 	\
	(bitmap_bswap(1UL << (BITMAP_WORD_BITS - ((_n) % BITMAP_WORD_BITS) - 1)))

#define BITMAP_HEADWORDS(_nbits) \
	((_nbits) / BITMAP_WORD_BITS)
#define BITMAP_HEADBYTES(_nbits) \
	(BITMAP_HEADWORDS(_nbits) * sizeof(bitmap_word))

#define BITMAP_TAILWORD(_bm, _nbits) \
	((_bm)[BITMAP_HEADWORDS(_nbits)].w)
#define BITMAP_HASTAIL(_nbits)	(((_nbits) % BITMAP_WORD_BITS) != 0)
#define BITMAP_TAILBITS(_nbits)	\
	(bitmap_bswap(~(-1UL >> ((_nbits) % BITMAP_WORD_BITS))))
#define BITMAP_TAIL(_bm, _nbits) \
	(BITMAP_TAILWORD(_bm, _nbits) & BITMAP_TAILBITS(_nbits))

static inline void bitmap_set_bit(bitmap *bitmap, int n)
{
	BITMAP_WORD(bitmap, n) |= BITMAP_WORDBIT(n);
}

static inline void bitmap_clear_bit(bitmap *bitmap, int n)
{
	BITMAP_WORD(bitmap, n) &= ~BITMAP_WORDBIT(n);
}

static inline void bitmap_change_bit(bitmap *bitmap, int n)
{
	BITMAP_WORD(bitmap, n) ^= BITMAP_WORDBIT(n);
}

static inline bool bitmap_test_bit(bitmap *bitmap, int n)
{
	return !!(BITMAP_WORD(bitmap, n) & BITMAP_WORDBIT(n));
}


static inline void bitmap_zero(bitmap *bitmap, int nbits)
{
	memset(bitmap, 0, bitmap_sizeof(nbits));
}

static inline void bitmap_fill(bitmap *bitmap, int nbits)
{
	memset(bitmap, 0xff, bitmap_sizeof(nbits));
}

static inline void bitmap_copy(bitmap *dst, bitmap *src, int nbits)
{
	memcpy(dst, src, bitmap_sizeof(nbits));
}

#define BITMAP_DEF_BINOP(_name, _op) \
	static inline void bitmap_##_name(bitmap *dst, bitmap *src1, bitmap *src2, \
					 int nbits) \
	{ \
		int i = 0; \
		for (i = 0; i < BITMAP_NWORDS(nbits); i++) { \
			dst[i].w = src1[i].w _op src2[i].w; \
		} \
	}

BITMAP_DEF_BINOP(and, &)
BITMAP_DEF_BINOP(or, |)
BITMAP_DEF_BINOP(xor, ^)
BITMAP_DEF_BINOP(andnot, & ~)

#undef BITMAP_DEF_BINOP

static inline void bitmap_complement(bitmap *dst, bitmap *src, int nbits)
{
	int i;

	for (i = 0; i < BITMAP_NWORDS(nbits); i++)
		dst[i].w = ~src[i].w;
}

static inline bool bitmap_equal(bitmap *src1, bitmap *src2, int nbits)
{
	return (memcmp(src1, src2, BITMAP_HEADBYTES(nbits)) == 0)
		&& (!BITMAP_HASTAIL(nbits)
		    || (BITMAP_TAIL(src1, nbits) == BITMAP_TAIL(src2, nbits)));
}

static inline bool bitmap_intersects(bitmap *src1, bitmap *src2, int nbits)
{
	int i;

	for (i = 0; i < BITMAP_HEADWORDS(nbits); i++) {
		if (src1[i].w & src2[i].w)
			return true;
	}
	if (BITMAP_HASTAIL(nbits) &&
	    (BITMAP_TAIL(src1, nbits) & BITMAP_TAIL(src2, nbits)))
		return true;
	return false;
}

static inline bool bitmap_subset(bitmap *src1, bitmap *src2, int nbits)
{
	int i;

	for (i = 0; i < BITMAP_HEADWORDS(nbits); i++) {
		if (src1[i].w  & ~src2[i].w)
			return false;
	}
	if (BITMAP_HASTAIL(nbits) &&
	    (BITMAP_TAIL(src1, nbits) & ~BITMAP_TAIL(src2, nbits)))
		return false;
	return true;
}

static inline bool bitmap_full(bitmap *bitmap, int nbits)
{
	int i;

	for (i = 0; i < BITMAP_HEADWORDS(nbits); i++) {
		if (bitmap[i].w != -1UL)
			return false;
	}
	if (BITMAP_HASTAIL(nbits) &&
	    (BITMAP_TAIL(bitmap, nbits) != BITMAP_TAILBITS(nbits)))
		return false;

	return true;
}

static inline bool bitmap_empty(bitmap *bitmap, int nbits)
{
	int i;

	for (i = 0; i < BITMAP_HEADWORDS(nbits); i++) {
		if (bitmap[i].w != 0)
			return false;
	}
	if (BITMAP_HASTAIL(nbits) && (BITMAP_TAIL(bitmap, nbits) != 0))
		return false;

	return true;
}

#endif /* CCAN_BITMAP_H_ */
