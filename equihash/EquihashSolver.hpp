// Equihash solver
// Copyright (c) 2016 John Tromp

// Fix N, K, such that n = N/(k+1) is integer
// Fix M = 2^{n+1} hashes each of length N bits,
// H_0, ... , H_{M-1}, generated fom (n+1)-bit indices.
// Problem: find binary tree on 2^K distinct indices,
// for which the exclusive-or of leaf hashes is all 0s.
// Additionally, it should satisfy the Wagner conditions:
// for each height i subtree, the exclusive-or
// of its 2^i corresponding hashes starts with i*n 0 bits,
// and for i>0 the leftmost leaf of its left subtree
// is less than the leftmost leaf of its right subtree

// The algorithm below solves this by maintaining the trees
// in a graph of K layers, each split into buckets
// with buckets indexed by the first n-RESTBITS bits following
// the i*n 0s, each bucket having 4 * 2^RESTBITS slots,
// twice the number of subtrees expected to land there.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "blake2/blake2.h"
#include <stdint.h> // for types uint32_t,uint64_t
#include <string.h> // for functions memset
#include <stdlib.h> // for function qsort
#include <stdbool.h>

typedef uint32_t u32;
typedef unsigned char uchar;

// algorithm parameters, prefixed with W to reduce include file conflicts

#ifndef WN
#define WN	200
#endif

#ifndef WK
#define WK	9
#endif

#define PARAMETER_N WN
#define PARAMETER_K WK

#define NDIGITS		(WK+1)
#define DIGITBITS	(WN/(NDIGITS))

#define PROOFSIZE (1<<WK)
#define BASE (1<<DIGITBITS)
#define NHASHES (2*BASE)
#define HASHESPERBLAKE (512/WN)
#define HASHOUT (HASHESPERBLAKE*WN/8)

typedef u32 proof[PROOFSIZE];

static void setheader(blake2b_state *ctx, const char *header, const u32 headerLen, const char* nce, const u32 nonceLen)
{
	uint32_t le_N = WN;
	uint32_t le_K = WK;
	uchar personal[] = "ZcashPoW01230123";
	memcpy(personal + 8, &le_N, 4);
	memcpy(personal + 12, &le_K, 4);
	blake2b_param P[1];
	P->digest_length = HASHOUT;
	P->key_length = 0;
	P->fanout = 1;
	P->depth = 1;
	P->leaf_length = 0;
	P->node_offset = 0;
	P->node_depth = 0;
	P->inner_length = 0;
	memset(P->reserved, 0, sizeof(P->reserved));
	memset(P->salt, 0, sizeof(P->salt));
	memcpy(P->personal, (const uint8_t *)personal, 16);
	blake2b_init_param(ctx, P);
	blake2b_update(ctx, (const uchar *)header, headerLen);
	if (nonceLen) {
		blake2b_update(ctx, (const uchar *)nce, nonceLen);
	}
}

enum verify_code { POW_OK, POW_DUPLICATE, POW_OUT_OF_ORDER, POW_NONZERO_XOR };
static const char *errstr[] = { "OK", "duplicate index", "indices out of order", "nonzero xor" };

static void genhash(blake2b_state *ctx, u32 idx, uchar *hash)
{
	blake2b_state state = *ctx;
	u32 leb = (idx / HASHESPERBLAKE);
	blake2b_update(&state, (uchar *)&leb, sizeof(u32));
	uchar blakehash[HASHOUT];
	blake2b_final(&state, blakehash, HASHOUT);
	memcpy(hash, blakehash + (idx % HASHESPERBLAKE) * WN / 8, WN / 8);
}

static int verifyrec(blake2b_state *ctx, u32 *indices, uchar *hash, int r)
{
	if (r == 0) {
		genhash(ctx, *indices, hash);
		return POW_OK;
	}
	u32 *indices1 = indices + (1ull << (r - 1));
	if (*indices >= *indices1) {
		return POW_OUT_OF_ORDER;
	}
	uchar hash0[WN / 8], hash1[WN / 8];
	int vrf0 = verifyrec(ctx, indices, hash0, r - 1);
	if (vrf0 != POW_OK) {
		return vrf0;
	}
	int vrf1 = verifyrec(ctx, indices1, hash1, r - 1);
	if (vrf1 != POW_OK) {
		return vrf1;
	}
	for (int i = 0; i < WN / 8; i++) {
		hash[i] = hash0[i] ^ hash1[i];
	}
	int i, b = r * DIGITBITS;
	for (i = 0; i < b / 8; i++) {
		if (hash[i]) {
			return POW_NONZERO_XOR;
		}
	}
	if ((b % 8) && hash[i] >> (8 - (b % 8))) {
		return POW_NONZERO_XOR;
	}
	return POW_OK;
}

static int compu32(const void *pa, const void *pb)
{
	u32 a = *(u32 *)pa, b = *(u32 *)pb;
	return a<b ? -1 : a == b ? 0 : +1;
}

static bool duped(proof prf)
{
	proof sortprf;
	memcpy(sortprf, prf, sizeof(proof));
	qsort(sortprf, PROOFSIZE, sizeof(u32), &compu32);
	for (u32 i = 1; i < PROOFSIZE; i++) {
		if (sortprf[i] <= sortprf[i - 1]) {
			return true;
		}
	}
	return false;
}

// verify Wagner conditions
static int verify(u32 indices[PROOFSIZE], const char *header, const u32 headerlen, const char *nonce, u32 noncelen)
{
	if (duped(indices)) {
		return POW_DUPLICATE;
	}
	blake2b_state ctx;
	setheader(&ctx, header, headerlen, nonce, noncelen);
	uchar hash[WN / 8];
	return verifyrec(&ctx, indices, hash, WK);
}

typedef uint16_t u16;
typedef uint64_t u64;

#ifdef ATOMIC
#include <atomic>
typedef std::atomic<u32> au32;
#else
typedef u32 au32;
#endif

#ifndef RESTBITS
#define RESTBITS	8
#endif

// 2_log of number of buckets
#define BUCKBITS (DIGITBITS-RESTBITS)

// number of buckets
static const u32 NBUCKETS = 1<<BUCKBITS;
// 2_log of number of slots per bucket
static const u32 SLOTBITS = RESTBITS+1+1;
// number of slots per bucket
static const u32 NSLOTS = 1<<SLOTBITS;
// number of per-xhash slots
static const u32 XFULL = 12;
// SLOTBITS mask
static const u32 SLOTMASK = NSLOTS-1;
// number of possible values of xhash (rest of n) bits
static const u32 NRESTS = 1<<RESTBITS;
// number of blocks of hashes extracted from single 512 bit blake2b output
static const u32 NBLOCKS = (NHASHES+HASHESPERBLAKE-1)/HASHESPERBLAKE;
// nothing larger found in 100000 runs
static const u32 MAXSOLS = 8;

// tree node identifying its children as two different slots in
// a bucket on previous layer with the same rest bits (x-tra hash)
struct tree {
	unsigned bucketid : BUCKBITS;
	unsigned slotid0 : SLOTBITS;
	unsigned slotid1 : SLOTBITS;

	// layer 0 has no children bit needs to encode index
	u32 getindex() const {
		return (bucketid << SLOTBITS) | slotid0;
	}
	
	void setindex(const u32 idx) {
		slotid0 = idx & SLOTMASK;
		bucketid = idx >> SLOTBITS;
	}
};

union hashunit {
	u32 word;
	uchar bytes[sizeof(u32)];
};

#define WORDS(bits)	((bits + 31) / 32)
#define HASHWORDS0 WORDS(WN - DIGITBITS + RESTBITS)
#define HASHWORDS1 WORDS(WN - 2*DIGITBITS + RESTBITS)

struct slot0 {
	tree attr;
	hashunit hash[HASHWORDS0];
};

struct slot1 {
	tree attr;
	hashunit hash[HASHWORDS1];
};

// a bucket is NSLOTS treenodes
typedef slot0 bucket0[NSLOTS];
typedef slot1 bucket1[NSLOTS];
// the N-bit hash consists of K+1 n-bit "digits"
// each of which corresponds to a layer of NBUCKETS buckets
typedef bucket0 digit0[NBUCKETS];
typedef bucket1 digit1[NBUCKETS];

// size (in bytes) of hash in round 0 <= r < WK
inline u32 hashsize(const u32 r) {
	const u32 hashbits = WN - (r + 1) * DIGITBITS + RESTBITS;
	return (hashbits + 7) / 8;
}

inline u32 hashwords(u32 bytes) {
	return (bytes + 3) / 4;
}

// manages hash and tree data
struct htalloc
{
	u32 *heap0;
	u32 *heap1;
	bucket0 *trees0[(WK + 1) / 2];
	bucket1 *trees1[WK / 2];
	u32 alloced;

	htalloc()
	{
		alloced = 0;
	}

	void alloctrees()
	{
		// optimize xenoncat's fixed memory layout, avoiding any waste
		// digit  trees  hashes  trees hashes
		// 0      0 A A A A A A   . . . . . .
		// 1      0 A A A A A A   1 B B B B B
		// 2      0 2 C C C C C   1 B B B B B
		// 3      0 2 C C C C C   1 3 D D D D
		// 4      0 2 4 E E E E   1 3 D D D D
		// 5      0 2 4 E E E E   1 3 5 F F F
		// 6      0 2 4 6 . G G   1 3 5 F F F
		// 7      0 2 4 6 . G G   1 3 5 7 H H
		// 8      0 2 4 6 8 . I   1 3 5 7 H H
		assert(DIGITBITS >= 16); // ensures hashes shorten by 1 unit every 2 digits
		heap0 = (u32 *)alloc(1, sizeof(digit0));
		heap1 = (u32 *)alloc(1, sizeof(digit1));
		for (int r = 0; r < WK; r++) {
			if ((r & 1) == 0) {
				trees0[r / 2] = (bucket0 *)(heap0 + r / 2);
			}
			else {
				trees1[r / 2] = (bucket1 *)(heap1 + r / 2);
			}
		}
	}

	void dealloctrees()
	{
		free(heap0);
		free(heap1);
	}

	void *alloc(const u32 n, const u32 sz)
	{
		void *mem = calloc(n, sz);
		assert(mem);
		alloced += n * sz;
		return mem;
	}
};

typedef au32 bsizes[NBUCKETS];

//u32 min(const u32 a, const u32 b) {
//  return a < b ? a : b;
//}

struct EquihashSolver
{
	blake2b_state blake_ctx;
	htalloc hta;
	bsizes *nslots; // PUT IN BUCKET STRUCT
	proof *sols;
	au32 nsols;
	u32 nthreads;
	u32 xfull;
	u32 hfull;
	u32 bfull;

	EquihashSolver(const u32 n_threads)
	{
		assert(sizeof(hashunit) == 4);
		nthreads = n_threads;
		hta.alloctrees();
		nslots = (bsizes *)hta.alloc(2 * NBUCKETS, sizeof(au32));
		sols = (proof *)hta.alloc(MAXSOLS, sizeof(proof));
	}

	~EquihashSolver()
	{
		hta.dealloctrees();
		free(nslots);
		free(sols);
	}

	void setnonce(const char *header, const u32 headerLen, const char* nonce, u32 nonceLen)
	{
		setheader(&blake_ctx, header, headerLen, nonce, nonceLen);
		memset(nslots, 0, NBUCKETS * sizeof(au32)); // only nslots[0] needs zeroing
		nsols = 0;
	}

	u32 getslot(const u32 r, const u32 bucketi)
	{
#ifdef ATOMIC
		return std::atomic_fetch_add_explicit(&nslots[r & 1][bucketi], 1U, std::memory_order_relaxed);
#else
		return nslots[r & 1][bucketi]++;
#endif
	}

	u32 getnslots(const u32 r, const u32 bid) // SHOULD BE METHOD IN BUCKET STRUCT
	{
		au32 &nslot = nslots[r & 1][bid];
		const u32 n = min(nslot, NSLOTS);
		nslot = 0;
		return n;
	}

	void orderindices(u32 *indices, u32 size)
	{
		if (indices[0] > indices[size]) {
			for (u32 i = 0; i < size; i++) {
				const u32 tmp = indices[i];
				indices[i] = indices[size + i];
				indices[size + i] = tmp;
			}
		}
	}

	void listindices0(u32 r, const tree t, u32 *indices)
	{
		if (r == 0) {
			*indices = t.getindex();
			return;
		}
		const bucket1 &buck = hta.trees1[--r / 2][t.bucketid];
		const u32 size = 1 << r;
		u32 *indices1 = indices + size;
		listindices1(r, buck[t.slotid0].attr, indices);
		listindices1(r, buck[t.slotid1].attr, indices1);
		orderindices(indices, size);
	}

	void listindices1(u32 r, const tree t, u32 *indices)
	{
		const bucket0 &buck = hta.trees0[--r / 2][t.bucketid];
		const u32 size = 1 << r;
		u32 *indices1 = indices + size;
		listindices0(r, buck[t.slotid0].attr, indices);
		listindices0(r, buck[t.slotid1].attr, indices1);
		orderindices(indices, size);
	}

	void candidate(const tree t)
	{
		proof prf;
		listindices1(WK, t, prf); // assume WK odd
		qsort(prf, PROOFSIZE, sizeof(u32), &compu32);
		for (u32 i = 1; i < PROOFSIZE; i++) {
			if (prf[i] <= prf[i - 1]) {
				return;
			}
		}
#ifdef ATOMIC
		u32 soli = std::atomic_fetch_add_explicit(&nsols, 1U, std::memory_order_relaxed);
#else
		u32 soli = nsols++;
#endif
		if (soli < MAXSOLS) {
			listindices1(WK, t, sols[soli]); // assume WK odd
		}
	}

	struct htlayout
	{
		htalloc hta;
		u32 prevhashunits;
		u32 nexthashunits;
		u32 dunits;
		u32 prevbo;
		u32 nextbo;

		htlayout(EquihashSolver *eq, u32 r) : hta(eq->hta), prevhashunits(0), dunits(0)
		{
			u32 nexthashbytes = hashsize(r);
			nexthashunits = hashwords(nexthashbytes);
			prevbo = 0;
			nextbo = nexthashunits * sizeof(hashunit) - nexthashbytes; // 0-3
			if (r) {
				u32 prevhashbytes = hashsize(r - 1);
				prevhashunits = hashwords(prevhashbytes);
				prevbo = prevhashunits * sizeof(hashunit) - prevhashbytes; // 0-3
				dunits = prevhashunits - nexthashunits;
			}
		}

		u32 getxhash0(const slot0* pslot) const
		{
#if WN == 200 && RESTBITS == 4
			return pslot->hash->bytes[prevbo] >> 4;
#elif WN == 200 && RESTBITS == 8
			return (pslot->hash->bytes[prevbo] & 0xf) << 4 | pslot->hash->bytes[prevbo + 1] >> 4;
#elif WN == 144 && RESTBITS == 4
			return pslot->hash->bytes[prevbo] & 0xf;
#else
#error non implemented
#endif
		}

		u32 getxhash1(const slot1* pslot) const
		{
#if WN == 200 && RESTBITS == 4
			return pslot->hash->bytes[prevbo] & 0xf;
#elif WN == 200 && RESTBITS == 8
			return pslot->hash->bytes[prevbo];
#elif WN == 144 && RESTBITS == 4
			return pslot->hash->bytes[prevbo] & 0xf;
#else
#error non implemented
#endif
		}

		bool equal(const hashunit *hash0, const hashunit *hash1) const
		{
			return hash0[prevhashunits - 1].word == hash1[prevhashunits - 1].word;
		}
	};

	struct collisiondata
	{
#ifdef XBITMAP
#if NSLOTS > 64
#error cant use XBITMAP with more than 64 slots
#endif
		u64 xhashmap[NRESTS];
		u64 xmap;
#else
#if RESTBITS <= 6
		typedef uchar xslot;
#else
		typedef u16 xslot;
#endif
		xslot nxhashslots[NRESTS];
		xslot xhashslots[NRESTS][XFULL];
		xslot *xx;
		u32 n0;
		u32 n1;
#endif
		u32 s0;

		void clear()
		{
#ifdef XBITMAP
			memset(xhashmap, 0, NRESTS * sizeof(u64));
#else
			memset(nxhashslots, 0, NRESTS * sizeof(xslot));
#endif
		}

		bool addslot(u32 s1, u32 xh)
		{
#ifdef XBITMAP
			xmap = xhashmap[xh];
			xhashmap[xh] |= (u64)1 << s1;
			s0 = -1;
			return true;
#else
			n1 = (u32)nxhashslots[xh]++;
			if (n1 >= XFULL)
				return false;
			xx = xhashslots[xh];
			xx[n1] = s1;
			n0 = 0;
			return true;
#endif
		}
		
		bool nextcollision() const
		{
#ifdef XBITMAP
			return xmap != 0;
#else
			return n0 < n1;
#endif
		}
		
		u32 slot()
		{
#ifdef XBITMAP
			const u32 ffs = __builtin_ffsll(xmap);
			s0 += ffs; xmap >>= ffs;
			return s0;
#else
			return (u32)xx[n0++];
#endif
		}
	};

	void digit0(const u32 id)
	{
		uchar hash[HASHOUT];
		blake2b_state state;
		htlayout htl(this, 0);
		const u32 hashbytes = hashsize(0);
		for (u32 block = id; block < NBLOCKS; block += nthreads) {
			state = blake_ctx;
			u32 leb = htole32(block);
			blake2b_update(&state, (uchar *)&leb, sizeof(u32));
			blake2b_final(&state, hash, HASHOUT);
			for (u32 i = 0; i < HASHESPERBLAKE; i++) {
				const uchar *ph = hash + i * WN / 8;
#if BUCKBITS == 16 && RESTBITS == 4
				const u32 bucketid = ((u32)ph[0] << 8) | ph[1];
#elif BUCKBITS == 12 && RESTBITS == 8
				const u32 bucketid = ((u32)ph[0] << 4) | ph[1] >> 4;
#elif BUCKBITS == 20 && RESTBITS == 4
				const u32 bucketid = ((((u32)ph[0] << 8) | ph[1]) << 4) | ph[2] >> 4;
#elif BUCKBITS == 12 && RESTBITS == 4
				const u32 bucketid = ((u32)ph[0] << 4) | ph[1] >> 4;
				const u32 xhash = ph[1] & 0xf;
#else
#error not implemented
#endif
				const u32 slot = getslot(0, bucketid);
				if (slot >= NSLOTS) {
					bfull++;
					continue;
				}
				tree leaf;
				leaf.setindex(block*HASHESPERBLAKE + i);
				slot0 &s = hta.trees0[0][bucketid][slot];
				s.attr = leaf;
				memcpy(s.hash->bytes + htl.nextbo, ph + WN / 8 - hashbytes, hashbytes);
			}
		}
	}

	void digitodd(const u32 r, const u32 id)
	{
		htlayout htl(this, r);
		collisiondata cd;
		for (u32 bucketid = id; bucketid < NBUCKETS; bucketid += nthreads) {
			cd.clear();
			slot0 *buck = htl.hta.trees0[(r - 1) / 2][bucketid]; // optimize by updating previous buck?!
			u32 bsize = getnslots(r - 1, bucketid);       // optimize by putting bucketsize with block?!
			for (u32 s1 = 0; s1 < bsize; s1++) {
				const slot0 *pslot1 = buck + s1;          // optimize by updating previous pslot1?!
				if (!cd.addslot(s1, htl.getxhash0(pslot1))) {
					xfull++;
					continue;
				}
				for (; cd.nextcollision(); ) {
					const u32 s0 = cd.slot();
					const slot0 *pslot0 = buck + s0;
					if (htl.equal(pslot0->hash, pslot1->hash)) {
						hfull++;
						continue;
					}
					u32 xorbucketid;
					const uchar *bytes0 = pslot0->hash->bytes, *bytes1 = pslot1->hash->bytes;
#if WN == 200 && BUCKBITS == 12 && RESTBITS == 8
					xorbucketid = (((u32)(bytes0[htl.prevbo + 1] ^ bytes1[htl.prevbo + 1]) & 0xf) << 8)
						| (bytes0[htl.prevbo + 2] ^ bytes1[htl.prevbo + 2]);
#elif WN == 144 && BUCKBITS == 20 && RESTBITS == 4
					xorbucketid = ((((u32)(bytes0[htl.prevbo + 1] ^ bytes1[htl.prevbo + 1]) << 8)
						| (bytes0[htl.prevbo + 2] ^ bytes1[htl.prevbo + 2])) << 4)
						| (bytes0[htl.prevbo + 3] ^ bytes1[htl.prevbo + 3]) >> 4;
#elif WN == 96 && BUCKBITS == 12 && RESTBITS == 4
					xorbucketid = ((u32)(bytes0[htl.prevbo + 1] ^ bytes1[htl.prevbo + 1]) << 4)
						| (bytes0[htl.prevbo + 2] ^ bytes1[htl.prevbo + 2]) >> 4;
#else
#error not implemented
#endif
					const u32 xorslot = getslot(r, xorbucketid);
					if (xorslot >= NSLOTS) {
						bfull++;
						continue;
					}
					tree xort; xort.bucketid = bucketid;
					xort.slotid0 = s0; xort.slotid1 = s1;
					slot1 &xs = htl.hta.trees1[r / 2][xorbucketid][xorslot];
					xs.attr = xort;
					for (u32 i = htl.dunits; i < htl.prevhashunits; i++)
						xs.hash[i - htl.dunits].word = pslot0->hash[i].word ^ pslot1->hash[i].word;
				}
			}
		}
	}

	void digiteven(const u32 r, const u32 id)
	{
		htlayout htl(this, r);
		collisiondata cd;
		for (u32 bucketid = id; bucketid < NBUCKETS; bucketid += nthreads) {
			cd.clear();
			slot1 *buck = htl.hta.trees1[(r - 1) / 2][bucketid]; // OPTIMIZE BY UPDATING PREVIOUS
			u32 bsize = getnslots(r - 1, bucketid);
			for (u32 s1 = 0; s1 < bsize; s1++) {
				const slot1 *pslot1 = buck + s1;          // OPTIMIZE BY UPDATING PREVIOUS
				if (!cd.addslot(s1, htl.getxhash1(pslot1))) {
					xfull++;
					continue;
				}
				for (; cd.nextcollision(); ) {
					const u32 s0 = cd.slot();
					const slot1 *pslot0 = buck + s0;
					if (htl.equal(pslot0->hash, pslot1->hash)) {
						hfull++;
						continue;
					}
					u32 xorbucketid;
					const uchar *bytes0 = pslot0->hash->bytes, *bytes1 = pslot1->hash->bytes;
#if WN == 200 && BUCKBITS == 12 && RESTBITS == 8
					xorbucketid = ((u32)(bytes0[htl.prevbo + 1] ^ bytes1[htl.prevbo + 1]) << 4)
						| (bytes0[htl.prevbo + 2] ^ bytes1[htl.prevbo + 2]) >> 4;
#elif WN == 144 && BUCKBITS == 20 && RESTBITS == 4
					xorbucketid = ((((u32)(bytes0[htl.prevbo + 1] ^ bytes1[htl.prevbo + 1]) << 8)
						| (bytes0[htl.prevbo + 2] ^ bytes1[htl.prevbo + 2])) << 4)
						| (bytes0[htl.prevbo + 3] ^ bytes1[htl.prevbo + 3]) >> 4;
#elif WN == 96 && BUCKBITS == 12 && RESTBITS == 4
					xorbucketid = ((u32)(bytes0[htl.prevbo + 1] ^ bytes1[htl.prevbo + 1]) << 4)
						| (bytes0[htl.prevbo + 2] ^ bytes1[htl.prevbo + 2]) >> 4;
#else
#error not implemented
#endif
					const u32 xorslot = getslot(r, xorbucketid);
					if (xorslot >= NSLOTS) {
						bfull++;
						continue;
					}
					tree xort; xort.bucketid = bucketid;
					xort.slotid0 = s0; xort.slotid1 = s1;
					slot0 &xs = htl.hta.trees0[r / 2][xorbucketid][xorslot];
					xs.attr = xort;
					for (u32 i = htl.dunits; i < htl.prevhashunits; i++)
						xs.hash[i - htl.dunits].word = pslot0->hash[i].word ^ pslot1->hash[i].word;
				}
			}
		}
	}

	void digitK(const u32 id)
	{
		collisiondata cd;
		htlayout htl(this, WK);
		for (u32 bucketid = id; bucketid < NBUCKETS; bucketid += nthreads) {
			cd.clear();
			slot0 *buck = htl.hta.trees0[(WK - 1) / 2][bucketid];
			u32 bsize = getnslots(WK - 1, bucketid);
			for (u32 s1 = 0; s1 < bsize; s1++) {
				const slot0 *pslot1 = buck + s1;
				if (!cd.addslot(s1, htl.getxhash0(pslot1))) // assume WK odd
					continue;
				for (; cd.nextcollision(); ) {
					const u32 s0 = cd.slot();
					const slot0 *pslot0 = buck + s0;
					if (htl.equal(pslot0->hash, pslot1->hash)) {
						tree xort; xort.bucketid = bucketid;
						xort.slotid0 = s0; xort.slotid1 = s1;
						candidate(xort);
					}
				}
			}
		}
	}
};
