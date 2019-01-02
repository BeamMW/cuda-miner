// BEAM CUDA Miner
// OpenCL Mining Sources for Equihash 150/5
// Copyright 2018 The Beam Team	
// Copyright 2018 Wilke Trei

#ifdef _WIN32
#include <Windows.h>
#endif
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <functional>
#include <vector>
#include <iostream>
#include <mutex>
#include <map>

#include "base/Logging.hpp"
#include "CudaSolver.hpp"
#include "sm_32_intrinsics.h"

// reduce vstudio warnings (__byteperm, blockIdx...)
#ifdef __INTELLISENSE__
#include <device_functions.h>
#include <device_launch_parameters.h>
#define __launch_bounds__(max_tpb, min_blocks)
#define __CUDA_ARCH__ 520
uint32_t __byte_perm(uint32_t x, uint32_t y, uint32_t z);
uint32_t __byte_perm(uint32_t x, uint32_t y, uint32_t z);
uint32_t __shfl(uint32_t x, uint32_t y, uint32_t z);
uint32_t atomicExch(uint32_t *x, uint32_t y);
uint32_t atomicAdd(uint32_t *x, uint32_t y);
void __syncthreads(void);
void __threadfence(void);
void __threadfence_block(void);
uint32_t __ldg(const uint32_t* address);
uint64_t __ldg(const uint64_t* address);
uint4 __ldca(const uint4 *ptr);
u32 __ldca(const u32 *ptr);
u32 umin(const u32, const u32);
u32 umax(const u32, const u32);
#endif

#define __shfl2(var, srcLane)  __shfl_sync(0xFFFFFFFFu, var, srcLane)
#undef __any
#define __any(p) __any_sync(0xFFFFFFFFu, p)

__device__ __constant__ const u64 blake_iv[] =
{
	0x6a09e667f3bcc908, 0xbb67ae8584caa73b,
	0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
	0x510e527fade682d1, 0x9b05688c2b3e6c1f,
	0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
};

__device__ __forceinline__ uint2 operator^ (uint2 a, uint2 b)
{
	return make_uint2(a.x ^ b.x, a.y ^ b.y);
}

__device__ __forceinline__ uint4 operator^ (uint4 a, uint4 b)
{
	return make_uint4(a.x ^ b.x, a.y ^ b.y, a.z ^ b.z, a.w ^ b.w);
}

__device__ __forceinline__ uint2 ROR2(const uint2 a, const int offset) 
{
	uint2 result;
	{
		asm("shf.r.wrap.b32 %0, %1, %2, %3;" : "=r"(result.x) : "r"(a.y), "r"(a.x), "r"(offset));
		asm("shf.r.wrap.b32 %0, %1, %2, %3;" : "=r"(result.y) : "r"(a.x), "r"(a.y), "r"(offset));
	}
	return result;
}

__device__ __forceinline__ uint2 SWAPUINT2(uint2 value) 
{
	return make_uint2(value.y, value.x);
}

__device__ __forceinline__ uint2 ROR24(const uint2 a)
{
	uint2 result;
	result.x = __byte_perm(a.y, a.x, 0x2107);
	result.y = __byte_perm(a.y, a.x, 0x6543);
	return result;
}

__device__ __forceinline__ uint2 ROR16(const uint2 a)
{
	uint2 result;
	result.x = __byte_perm(a.y, a.x, 0x1076);
	result.y = __byte_perm(a.y, a.x, 0x5432);
	return result;
}

__device__ __forceinline__ void G2(u64 & a, u64 & b, u64 & c, u64 & d, u64 x, u64 y) 
{
	a = a + b + x;
	((uint2*)&d)[0] = SWAPUINT2(((uint2*)&d)[0] ^ ((uint2*)&a)[0]);
	c = c + d;
	((uint2*)&b)[0] = ROR24(((uint2*)&b)[0] ^ ((uint2*)&c)[0]);
	a = a + b + y;
	((uint2*)&d)[0] = ROR16(((uint2*)&d)[0] ^ ((uint2*)&a)[0]);
	c = c + d;
	((uint2*)&b)[0] = ROR2(((uint2*)&b)[0] ^ ((uint2*)&c)[0], 63U);
}

__device__ __forceinline__ void shr_5(uint4 &lo, uint4 &hi, u32 bits)
{
	const u32 lbits = 32 - bits;
	uint4 tmpLo;
	uint4 tmpHi;

	tmpLo.x = lo.x << lbits;
	tmpLo.y = lo.y << lbits;
	tmpLo.z = lo.z << lbits;
	tmpLo.w = lo.w << lbits;

	tmpHi.x = hi.x << lbits;

	lo.x = lo.x >> bits;
	lo.y = lo.y >> bits;
	lo.z = lo.z >> bits;
	lo.w = lo.w >> bits;

	hi.x = hi.x >> bits;

	lo.x |= tmpLo.y;
	lo.y |= tmpLo.z;
	lo.z |= tmpLo.w;
	lo.w |= tmpHi.x;
}

__device__ __forceinline__ void shr_4(uint4 &lo, u32 bits)
{
	const u32 lbits = 32 - bits;
	uint4 tmpLo;

	tmpLo.x = lo.x << lbits;
	tmpLo.y = lo.y << lbits;
	tmpLo.z = lo.z << lbits;
	tmpLo.w = lo.w << lbits;

	lo.x = lo.x >> bits;
	lo.y = lo.y >> bits;
	lo.z = lo.z >> bits;
	lo.w = lo.w >> bits;

	lo.x |= tmpLo.y;
	lo.y |= tmpLo.z;
	lo.z |= tmpLo.w;
}

/*
This function swaps the order of bits in each byte from low to high endian.
This is required for having the xor bits in right order
*/
__device__ __forceinline__ u32 swapBitOrder(u32 input)
{
	u32 tmp0 = input & 0x0F0F0F0F;
	u32 tmp1 = input & 0xF0F0F0F0;

	tmp0 = tmp0 << 4;
	tmp1 = tmp1 >> 4;

	u32 tmpIn = tmp0 | tmp1;

	tmp0 = tmpIn & 0x33333333;
	tmp1 = tmpIn & 0xCCCCCCCC;

	tmp0 = tmp0 << 2;
	tmp1 = tmp1 >> 2;

	tmpIn = tmp0 | tmp1;

	tmp0 = tmpIn & 0x55555555;
	tmp1 = tmpIn & 0xAAAAAAAA;

	tmp0 = tmp0 << 1;
	tmp1 = tmp1 >> 1;

	return tmp0 | tmp1;
}

__global__ void round_init(uint4 *outputLo, uint2 *outputHi, u32 *counters, u64 blockHeader1, u64 blockHeader2, u64 blockHeader3, u64 blockHeader4, u64 nonce)
{
	const u32 block = blockIdx.x * blockDim.x + threadIdx.x;

	union
	{
		u64 m[8];
		u32 m32[16];
	};

	{
		u64 blakeState[8];

		blakeState[0] = blake_iv[0] ^ (0x01010000 | 57);	// We want to read 57 bytes from each blake call

		blakeState[1] = blake_iv[1];
		blakeState[2] = blake_iv[2];
		blakeState[3] = blake_iv[3];
		blakeState[4] = blake_iv[4];
		blakeState[5] = blake_iv[5];

		blakeState[6] = blake_iv[6] ^ 0x576F502D6D616542ull;   // Equals personalization string "Beam-PoW"

		blakeState[7] = blake_iv[7] ^ 0x0000000500000096ull; // k, n

		union
		{
			u64 v[16];
			u32 v32[32];
			uint4 v128[8];
		};

		v[0] = blakeState[0];
		v[1] = blakeState[1];
		v[2] = blakeState[2];
		v[3] = blakeState[3];
		v[4] = blakeState[4];
		v[5] = blakeState[5];
		v[6] = blakeState[6];
		v[7] = blakeState[7];
		v[8] = blake_iv[0];
		v[9] = blake_iv[1];
		v[10] = blake_iv[2];
		v[11] = blake_iv[3];
		v[12] = blake_iv[4] ^ (32 + 8 + 4); // input + nonce + block
		v[13] = blake_iv[5];
		v[14] = blake_iv[6] ^ 0xffffffffffffffff;
		v[15] = blake_iv[7];

		m[0] = blockHeader1;
		m[1] = blockHeader2;
		m[2] = blockHeader3;
		m[3] = blockHeader4;

		m[4] = nonce;
		m[5] = block;

		// mix 1
		G2(v[0], v[4], v[8], v[12], m[0], m[1]);
		G2(v[1], v[5], v[9], v[13], m[2], m[3]);
		G2(v[2], v[6], v[10], v[14], m[4], m[5]);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 2
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], m[4], 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], m[1], 0);
		G2(v[1], v[6], v[11], v[12], m[0], m[2]);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], m[5], m[3]);

		// mix 3
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, m[0]);
		G2(v[2], v[6], v[10], v[14], m[5], m[2]);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], m[3], 0);
		G2(v[2], v[7], v[8], v[13], 0, m[1]);
		G2(v[3], v[4], v[9], v[14], 0, m[4]);

		// mix 4
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], m[3], m[1]);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], m[2], 0);
		G2(v[1], v[6], v[11], v[12], m[5], 0);
		G2(v[2], v[7], v[8], v[13], m[4], m[0]);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 5
		G2(v[0], v[4], v[8], v[12], 0, m[0]);
		G2(v[1], v[5], v[9], v[13], m[5], 0);
		G2(v[2], v[6], v[10], v[14], m[2], m[4]);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, m[1]);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], m[3], 0);

		// mix 6
		G2(v[0], v[4], v[8], v[12], m[2], 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], m[0], 0);
		G2(v[3], v[7], v[11], v[15], 0, m[3]);
		G2(v[0], v[5], v[10], v[15], m[4], 0);
		G2(v[1], v[6], v[11], v[12], 0, m[5]);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], m[1], 0);

		// mix 7
		G2(v[0], v[4], v[8], v[12], 0, m[5]);
		G2(v[1], v[5], v[9], v[13], m[1], 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], m[4], 0);
		G2(v[0], v[5], v[10], v[15], m[0], 0);
		G2(v[1], v[6], v[11], v[12], 0, m[3]);
		G2(v[2], v[7], v[8], v[13], 0, m[2]);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 8
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, m[1]);
		G2(v[3], v[7], v[11], v[15], m[3], 0);
		G2(v[0], v[5], v[10], v[15], m[5], m[0]);
		G2(v[1], v[6], v[11], v[12], 0, m[4]);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], m[2], 0);

		// mix 9
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, m[3]);
		G2(v[3], v[7], v[11], v[15], m[0], 0);
		G2(v[0], v[5], v[10], v[15], 0, m[2]);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], m[1], m[4]);
		G2(v[3], v[4], v[9], v[14], 0, m[5]);

		// mix 10
		G2(v[0], v[4], v[8], v[12], 0, m[2]);
		G2(v[1], v[5], v[9], v[13], 0, m[4]);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], m[1], m[5]);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], m[3], 0);
		G2(v[3], v[4], v[9], v[14], 0, m[0]);

		// mix 11
		G2(v[0], v[4], v[8], v[12], m[0], m[1]);
		G2(v[1], v[5], v[9], v[13], m[2], m[3]);
		G2(v[2], v[6], v[10], v[14], m[4], m[5]);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 12
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], m[4], 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], m[1], 0);
		G2(v[1], v[6], v[11], v[12], m[0], m[2]);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], m[5], m[3]);

		v[0] ^= blakeState[0] ^ v[8];
		v[1] ^= blakeState[1] ^ v[9];
		v[2] ^= blakeState[2] ^ v[10];
		v[3] ^= blakeState[3] ^ v[11];
		v[4] ^= blakeState[4] ^ v[12];
		v[5] ^= blakeState[5] ^ v[13];
		v[6] ^= blakeState[6] ^ v[14];
		v[7] ^= blakeState[7] ^ v[15];

		__syncthreads();

		u32 startIndex = block & 0xFFFFFFF0;

		for (u32 idx = 0; idx < 16; idx++) {
			m32[idx] = 0;
			for (u32 g2 = startIndex; g2 <= block; g2++) {
				m32[idx] += __shfl_sync(0xFFFFFFFF, v32[idx], g2, 16);
			}
			m32[idx] = swapBitOrder(m32[idx]);
		}
	}

	union {
		uint4	s4[2];
		uint2	z[4];
		u32		s32[8];
	} output;
	u32 bucket;
	u32 pos;

	output.s32[0] = m32[0]; 							// First element are bytes 0 to 18 
	output.s32[1] = m32[1];
	output.s32[2] = m32[2];
	output.s32[3] = m32[3];
	output.s32[4] = m32[4] & 0x3FFFFF;  	  					// Only lower 22 bits  
	output.s32[5] = (block << 1) + block;
	/*
	We will sort the element into 2^13
	buckets of maximal size 8672
	*/
	bucket = output.s32[0] & 0x1FFF;
	pos = atomicAdd(&counters[bucket], 1);
	shr_5(output.s4[0], output.s4[1], 13);

	outputLo[bucket * 8672 + pos] = output.s4[0];
	outputHi[bucket * 8672 + pos] = output.z[2];

	output.s32[0] = (m32[4] >> 24) | (m32[5] << 8); 				// Second element are bytes 19 to 37 
	output.s32[1] = (m32[5] >> 24) | (m32[6] << 8);
	output.s32[2] = (m32[6] >> 24) | (m32[7] << 8);
	output.s32[3] = (m32[7] >> 24) | (m32[8] << 8);
	output.s32[4] = ((m32[8] >> 24) | (m32[9] << 8)) & 0x3FFFFF;			// Only lower 22 bits 
	output.s32[5]++;

	bucket = output.s32[0] & 0x1FFF;
	pos = atomicAdd(&counters[bucket], 1);
	shr_5(output.s4[0], output.s4[1], 13);

	outputLo[bucket * 8672 + pos] = output.s4[0];
	outputHi[bucket * 8672 + pos] = output.z[2];

	output.s32[0] = (m32[9] >> 16) | (m32[10] << 16);  				// Third element are bytes 38 to 56
	output.s32[1] = (m32[10] >> 16) | (m32[11] << 16);
	output.s32[2] = (m32[11] >> 16) | (m32[12] << 16);
	output.s32[3] = (m32[12] >> 16) | (m32[13] << 16);
	output.s32[4] = ((m32[13] >> 16) | (m32[14] << 16)) & 0x3FFFFF;		// Only lower 22 bits 
	output.s32[5]++;

	bucket = output.s32[0] & 0x1FFF;
	pos = atomicAdd(&counters[bucket], 1);
	shr_5(output.s4[0], output.s4[1], 13);

	outputLo[bucket * 8672 + pos] = output.s4[0];
	outputHi[bucket * 8672 + pos] = output.z[2];
}

__device__ __forceinline__ void masking6(uint4 input0, uint2 input1, u32* scratch, u32* tab, u32* cnt, u32 mask) {
	if ((input0.x & 0x7) == mask) {
		u32 pos = atomicAdd(&cnt[0], 1);
		if (pos < 1216) {
			u32 value = atomicExch(&tab[(input0.x >> 3) & 0x1FF], pos);
			scratch[pos] = input0.x;
			scratch[1216 + pos] = input0.y;
			scratch[2432 + pos] = input0.z;
			scratch[3648 + pos] = input0.w;
			scratch[4864 + pos] = input1.x | (value << 16);	// Saving space in round 1
			scratch[6080 + pos] = input1.y;
		}
	}
}

__device__ __forceinline__ void masking4(uint4 input0, u32 id, u32* scratch, u32* tab, u32* cnt, u32 mask) {
	if ((input0.x & 0x7) == mask) {
		u32 pos = atomicAdd(&cnt[0], 1);
		if (pos < 1216) {
			u32 value = atomicExch(&tab[(input0.x >> 3) & 0x1FF], pos);
			scratch[pos] = input0.x;
			scratch[1216 + pos] = input0.y;
			scratch[2432 + pos] = input0.z;
			scratch[3648 + pos] = input0.w;
			scratch[4864 + pos] = value;
			scratch[6080 + pos] = id;
		}
	}
}

__global__ void round_1(
	uint4 * input0,
	uint2 * input1,
	uint4 * output0,
	uint2 * output1,
	u32 * counters)
{
	const u32 lId = threadIdx.x;
	const u32 grp = blockIdx.x;

	u32 bucket = grp >> 3;
	u32 mask = (grp & 7);

	__shared__ u32 scratch[7296];

	u32 * scratch0 = &scratch[0];
	u32 * scratch1 = &scratch[1216];
	u32 * scratch2 = &scratch[2432];
	u32 * scratch3 = &scratch[3648];
	u32 * scratch4 = &scratch[4864];
	u32 * scratch5 = &scratch[6080];

	__shared__ u32 tab[512];
	__shared__ u32 iCNT[2];

	u32 * inCounter = &counters[0];
	u32 * outCounter = &counters[8192];

	if (lId == 0) {
		iCNT[1] = 0;
		iCNT[0] = min(inCounter[bucket], (u32)8672);
	}

	tab[lId] = 0xFFF;
	tab[lId + 256] = 0xFFF;

	__syncthreads();

	u32 ofs = bucket * 8672;

	masking6(input0[ofs + lId], input1[ofs + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 256 + lId], input1[ofs + 256 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 512 + lId], input1[ofs + 512 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 768 + lId], input1[ofs + 768 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 1024 + lId], input1[ofs + 1024 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 1280 + lId], input1[ofs + 1280 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 1536 + lId], input1[ofs + 1536 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 1792 + lId], input1[ofs + 1792 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 2048 + lId], input1[ofs + 2048 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 2304 + lId], input1[ofs + 2304 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 2560 + lId], input1[ofs + 2560 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 2816 + lId], input1[ofs + 2816 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 3072 + lId], input1[ofs + 3072 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 3328 + lId], input1[ofs + 3328 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 3584 + lId], input1[ofs + 3584 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 3840 + lId], input1[ofs + 3840 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 4096 + lId], input1[ofs + 4096 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 4352 + lId], input1[ofs + 4352 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 4608 + lId], input1[ofs + 4608 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 4864 + lId], input1[ofs + 4864 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 5120 + lId], input1[ofs + 5120 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 5376 + lId], input1[ofs + 5376 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 5632 + lId], input1[ofs + 5632 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 5888 + lId], input1[ofs + 5888 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 6144 + lId], input1[ofs + 6144 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 6400 + lId], input1[ofs + 6400 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 6656 + lId], input1[ofs + 6656 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 6912 + lId], input1[ofs + 6912 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	masking6(input0[ofs + 7168 + lId], input1[ofs + 7168 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	masking6(input0[ofs + 7424 + lId], input1[ofs + 7424 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7680) < iCNT[0]) masking6(input0[ofs + 7680 + lId], input1[ofs + 7680 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7936) < iCNT[0]) masking6(input0[ofs + 7936 + lId], input1[ofs + 7936 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	if ((lId + 8192) < iCNT[0]) masking6(input0[ofs + 8192 + lId], input1[ofs + 8192 + lId], &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 8448) < iCNT[0]) masking6(input0[ofs + 8448 + lId], input1[ofs + 8448 + lId], &scratch[0], &tab[0], &iCNT[1], mask);

	__syncthreads();

	u32 inLim = min(iCNT[1], (u32)1216);

	__syncthreads();

	u32 ownPos = lId;
	u32 own = scratch4[ownPos];
	u32 othPos = own >> 16;
	u32 buck, pos;
	u32 cnt = 0;

	union {
		uint4	s4[2];
		uint2	z[4];
		u32		s32[8];
	} outputEl;

	while (ownPos < inLim) {
		u32 addr = (othPos < inLim) ? othPos : ownPos + 256;
		u32 elem = scratch4[addr];

		if (othPos < inLim) {
			outputEl.s32[0] = scratch0[ownPos] ^ scratch0[othPos];

			buck = (outputEl.s32[0] >> 12) & 0x1FFF;
			pos = atomicAdd(&outCounter[buck], 1);

			outputEl.s32[1] = scratch1[ownPos] ^ scratch1[othPos];
			outputEl.s32[2] = scratch2[ownPos] ^ scratch2[othPos];
			outputEl.s32[3] = scratch3[ownPos] ^ scratch3[othPos];
			outputEl.s32[4] = (own ^ elem) & 0x1FF;

			shr_5(outputEl.s4[0], outputEl.s4[1], 25); 			// Shift away 25 bits
			outputEl.s32[4] = scratch5[ownPos];
			outputEl.s32[5] = scratch5[othPos];

			pos += buck * 8672;

			output0[pos] = outputEl.s4[0];
			output1[pos] = outputEl.z[2];
		}
		else {
			own = elem;
			ownPos += 256;
		}

		othPos = (elem >> 16);
		ownPos = (cnt<40) ? ownPos : inLim;
		cnt++;
	}
}

__global__ void round_2(
	uint4 * input0,
	uint4 * output0,
	u32 * counters)
{
	const u32 lId = threadIdx.x;
	const u32 grp = blockIdx.x;

	u32 bucket = grp >> 3;
	u32 mask = (grp & 7);

	__shared__ u32 scratch[7296];

	u32 * scratch0 = &scratch[0];
	u32 * scratch1 = &scratch[1216];
	u32 * scratch2 = &scratch[2432];
	u32 * scratch3 = &scratch[3648];
	u32 * scratch4 = &scratch[4864];
	u32 * scratch5 = &scratch[6080];

	__shared__ u32 tab[512];
	__shared__ u32 iCNT[2];

	u32 * inCounter = &counters[8192];
	u32 * outCounter = &counters[16384];

	if (lId == 0) {
		iCNT[1] = 0;
		iCNT[0] = min(inCounter[bucket], (u32)8672);
	}

	tab[lId] = 0xFFF;
	tab[lId + 256] = 0xFFF;

	__syncthreads();

	u32 ofs = bucket * 8672;

	masking4(input0[ofs + lId], lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 256 + lId], 256 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 512 + lId], 512 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 768 + lId], 768 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 1024 + lId], 1024 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1280 + lId], 1280 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1536 + lId], 1536 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1792 + lId], 1792 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 2048 + lId], 2048 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2304 + lId], 2304 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2560 + lId], 2560 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2816 + lId], 2816 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 3072 + lId], 3072 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3328 + lId], 3328 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3584 + lId], 3584 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3840 + lId], 3840 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 4096 + lId], 4096 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4352 + lId], 4352 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4608 + lId], 4608 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4864 + lId], 4864 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 5120 + lId], 5120 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5376 + lId], 5376 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5632 + lId], 5632 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5888 + lId], 5888 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 6144 + lId], 6144 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6400 + lId], 6400 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6656 + lId], 6656 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6912 + lId], 6912 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 7168 + lId], 7168 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 7424 + lId], 7424 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7680) < iCNT[0]) masking4(input0[ofs + 7680 + lId], 7680 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7936) < iCNT[0]) masking4(input0[ofs + 7936 + lId], 7936 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	if ((lId + 8192) < iCNT[0]) masking4(input0[ofs + 8192 + lId], 8192 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 8448) < iCNT[0]) masking4(input0[ofs + 8448 + lId], 8448 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	__syncthreads();

	u32 inLim = min(iCNT[1], (u32)1216);

	__syncthreads();

	u32 ownPos = lId;
	u32 own = scratch4[ownPos];
	u32 othPos = own;
	u32 buck, pos;
	u32 cnt = 0;

	union {
		uint4	s4[2];
		uint2	z[4];
		u32		s32[8];
	} outputEl;

	while (ownPos < inLim) {
		u32 addr = (othPos < inLim) ? othPos : ownPos + 256;
		u32 elem = scratch4[addr];

		if (othPos < inLim) {
			outputEl.s32[0] = scratch0[ownPos] ^ scratch0[othPos];
			outputEl.s32[1] = scratch1[ownPos] ^ scratch1[othPos];
			if (outputEl.s32[1] != 0) {
				buck = (outputEl.s32[0] >> 12) & 0x1FFF;
				pos = atomicAdd(&outCounter[buck], 1);

				outputEl.s32[2] = scratch2[ownPos] ^ scratch2[othPos];
				outputEl.s32[3] = scratch3[ownPos] ^ scratch3[othPos];

				shr_4(outputEl.s4[0], 25); 			// Shift away 25 bits

				/*
				Remaining bits: 150-2*25-13 = 87
				Each element index has at most 14 bits
				Bucket index has 13 bits

				87 + 2*14 + 13 = 128
				Fits into uint4

				*/

				outputEl.s32[3] = scratch5[ownPos];
				outputEl.s32[3] |= (scratch5[othPos] << 14);
				outputEl.s32[3] |= (bucket << 28);

				outputEl.s32[2] |= (bucket >> 4) << 23;

				if (pos < 8672) {
					pos += buck * 8672;
					output0[pos] = outputEl.s4[0];
				}
			}
		}
		else {
			own = elem;
			ownPos += 256;
		}

		othPos = elem;
		ownPos = (cnt<40) ? ownPos : inLim;
		cnt++;
	}
}

__global__ void round_3(
	uint4 * input0,
	uint4 * output0,
	u32 * counters)
{
	const u32 lId = threadIdx.x;
	const u32 grp = blockIdx.x;

	u32 bucket = grp >> 3;
	u32 mask = (grp & 7);

	__shared__ u32 scratch[7296];

	u32 * scratch0 = &scratch[0];
	u32 * scratch1 = &scratch[1216];
	u32 * scratch2 = &scratch[2432];
	u32 * scratch3 = &scratch[3648];
	u32 * scratch4 = &scratch[4864];
	u32 * scratch5 = &scratch[6080];

	__shared__ u32 tab[512];
	__shared__ u32 iCNT[2];

	u32 * inCounter = &counters[16384];
	u32 * outCounter = &counters[24576];

	if (lId == 0) {
		iCNT[1] = 0;
		iCNT[0] = min(inCounter[bucket], (u32)8672);
	}

	tab[lId] = 0xFFF;
	tab[lId + 256] = 0xFFF;

	__syncthreads();

	u32 ofs = bucket * 8672;

	masking4(input0[ofs + lId], ofs + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 256 + lId], ofs + 256 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 512 + lId], ofs + 512 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 768 + lId], ofs + 768 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 1024 + lId], ofs + 1024 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1280 + lId], ofs + 1280 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1536 + lId], ofs + 1536 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1792 + lId], ofs + 1792 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 2048 + lId], ofs + 2048 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2304 + lId], ofs + 2304 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2560 + lId], ofs + 2560 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2816 + lId], ofs + 2816 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 3072 + lId], ofs + 3072 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3328 + lId], ofs + 3328 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3584 + lId], ofs + 3584 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3840 + lId], ofs + 3840 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 4096 + lId], ofs + 4096 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4352 + lId], ofs + 4352 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4608 + lId], ofs + 4608 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4864 + lId], ofs + 4864 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 5120 + lId], ofs + 5120 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5376 + lId], ofs + 5376 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5632 + lId], ofs + 5632 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5888 + lId], ofs + 5888 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 6144 + lId], ofs + 6144 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6400 + lId], ofs + 6400 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6656 + lId], ofs + 6656 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6912 + lId], ofs + 6912 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 7168 + lId], ofs + 7168 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 7424 + lId], ofs + 7424 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7680) < iCNT[0]) masking4(input0[ofs + 7680 + lId], ofs + 7680 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7936) < iCNT[0]) masking4(input0[ofs + 7936 + lId], ofs + 7936 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	if ((lId + 8192) < iCNT[0]) masking4(input0[ofs + 8192 + lId], ofs + 8192 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 8448) < iCNT[0]) masking4(input0[ofs + 8448 + lId], ofs + 8448 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	__syncthreads();

	u32 inLim = min(iCNT[1], (u32)1216);

	__syncthreads();

	u32 ownPos = lId;
	u32 own = scratch4[ownPos];
	u32 othPos = own;
	u32 buck, pos;
	u32 cnt = 0;

	union {
		uint4	s4[2];
		uint2	z[4];
		u32		s32[8];
	} outputEl;

	while (ownPos < inLim) {
		u32 addr = (othPos < inLim) ? othPos : ownPos + 256;
		u32 elem = scratch4[addr];

		if (othPos < inLim) {
			outputEl.s32[0] = scratch0[ownPos] ^ scratch0[othPos];
			outputEl.s32[1] = scratch1[ownPos] ^ scratch1[othPos];
			if (outputEl.s32[1] != 0) {
				buck = (outputEl.s32[0] >> 12) & 0x1FFF;
				pos = atomicAdd(&outCounter[buck], 1);

				outputEl.s32[2] = (scratch2[ownPos] ^ scratch2[othPos]) & 0x7FFFFF;
				outputEl.s32[3] = 0;

				shr_4(outputEl.s4[0], 25); 			// Shift away 25 bits

				/*
				Remaining bits: 150-3*25-13 = 62
				Addresses of inputs will be stored in 3rd and 4th component

				*/

				outputEl.s32[2] = scratch5[ownPos];
				outputEl.s32[3] = scratch5[othPos];

				if (pos < 8672) {
					pos += buck * 8672;
					output0[pos] = outputEl.s4[0];
				}
			}
		}
		else {
			own = elem;
			ownPos += 256;
		}

		othPos = elem;
		ownPos = (cnt<40) ? ownPos : inLim;
		cnt++;
	}
}

__global__ void round_4(
	uint4 * input0,
	uint4 * output0,
	u32 * counters)
{
	const u32 lId = threadIdx.x;
	const u32 grp = blockIdx.x;

	u32 bucket = grp >> 3;
	u32 mask = (grp & 7);

	__shared__ u32 scratch[7296];

	u32 * scratch0 = &scratch[0];
	u32 * scratch1 = &scratch[1216];
	u32 * scratch2 = &scratch[2432];
	u32 * scratch3 = &scratch[3648];
	u32 * scratch4 = &scratch[4864];
	u32 * scratch5 = &scratch[6080];

	__shared__ u32 tab[512];
	__shared__ u32 iCNT[2];

	u32 * inCounter = &counters[24576];
	u32 * outCounter = &counters[32768];

	if (lId == 0) {
		iCNT[1] = 0;
		iCNT[0] = min(inCounter[bucket], (u32)8672);
	}

	tab[lId] = 0xFFF;
	tab[lId + 256] = 0xFFF;

	__syncthreads();

	u32 ofs = bucket * 8672;

	masking4(input0[ofs + lId], ofs + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 256 + lId], ofs + 256 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 512 + lId], ofs + 512 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 768 + lId], ofs + 768 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 1024 + lId], ofs + 1024 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1280 + lId], ofs + 1280 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1536 + lId], ofs + 1536 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1792 + lId], ofs + 1792 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 2048 + lId], ofs + 2048 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2304 + lId], ofs + 2304 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2560 + lId], ofs + 2560 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2816 + lId], ofs + 2816 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 3072 + lId], ofs + 3072 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3328 + lId], ofs + 3328 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3584 + lId], ofs + 3584 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3840 + lId], ofs + 3840 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 4096 + lId], ofs + 4096 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4352 + lId], ofs + 4352 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4608 + lId], ofs + 4608 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4864 + lId], ofs + 4864 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 5120 + lId], ofs + 5120 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5376 + lId], ofs + 5376 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5632 + lId], ofs + 5632 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5888 + lId], ofs + 5888 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 6144 + lId], ofs + 6144 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6400 + lId], ofs + 6400 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6656 + lId], ofs + 6656 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6912 + lId], ofs + 6912 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 7168 + lId], ofs + 7168 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 7424 + lId], ofs + 7424 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7680) < iCNT[0]) masking4(input0[ofs + 7680 + lId], ofs + 7680 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7936) < iCNT[0]) masking4(input0[ofs + 7936 + lId], ofs + 7936 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	if ((lId + 8192) < iCNT[0]) masking4(input0[ofs + 8192 + lId], ofs + 8192 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 8448) < iCNT[0]) masking4(input0[ofs + 8448 + lId], ofs + 8448 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	__syncthreads();

	u32 inLim = min(iCNT[1], (u32)1216);

	__syncthreads();

	u32 ownPos = lId;
	u32 own = scratch4[ownPos];
	u32 othPos = own;
	u32 buck, pos;
	u32 cnt = 0;

	union {
		uint4	s4[2];
		uint2	z[4];
		u32		s32[8];
	} outputEl;

	while (ownPos < inLim) {
		u32 addr = (othPos < inLim) ? othPos : ownPos + 256;
		u32 elem = scratch4[addr];

		if (othPos < inLim) {
			outputEl.s32[0] = scratch0[ownPos] ^ scratch0[othPos];
			outputEl.s32[1] = scratch1[ownPos] ^ scratch1[othPos];
			if (outputEl.s32[1] != 0) {
				buck = (outputEl.s32[0] >> 12) & 0x1FFF;
				pos = atomicAdd(&outCounter[buck], 1);

				outputEl.s32[2] = 0;
				outputEl.s32[3] = 0;

				shr_4(outputEl.s4[0], 25); 			// Shift away 25 bits

				/*
				Remaining bits: 150-4*25-13 = 37
				Addresses of inputs will be stored in 3rd and 4th component

				*/

				outputEl.s32[2] = scratch5[ownPos];
				outputEl.s32[3] = scratch5[othPos];

				if (pos < 8672) {
					pos += buck * 8672;
					output0[pos] = outputEl.s4[0];
				}
			}
		}
		else {
			own = elem;
			ownPos += 256;
		}

		othPos = elem;
		ownPos = (cnt<40) ? ownPos : inLim;
		cnt++;
	}
}

__global__ void round_5(
	uint4 * input0,
	uint4 * output0,
	u32 * counters)
{
	const u32 lId = threadIdx.x;
	const u32 grp = blockIdx.x;

	u32 bucket = grp >> 3;
	u32 mask = (grp & 7);

	__shared__ u32 scratch[7296];

	u32 * scratch0 = &scratch[0];
	u32 * scratch1 = &scratch[1216];
	u32 * scratch2 = &scratch[2432];
	u32 * scratch3 = &scratch[3648];
	u32 * scratch4 = &scratch[4864];
	u32 * scratch5 = &scratch[6080];

	__shared__ u32 tab[512];
	__shared__ u32 iCNT[2];

	u32 * inCounter = &counters[32768];
	u32 * outCounter = &counters[40960];

	if (lId == 0) {
		iCNT[1] = 0;
		iCNT[0] = min(inCounter[bucket], (u32)8672);
	}

	tab[lId] = 0xFFF;
	tab[lId + 256] = 0xFFF;

	__syncthreads();

	u32 ofs = bucket * 8672;

	masking4(input0[ofs + lId], ofs + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 256 + lId], ofs + 256 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 512 + lId], ofs + 512 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 768 + lId], ofs + 768 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 1024 + lId], ofs + 1024 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1280 + lId], ofs + 1280 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1536 + lId], ofs + 1536 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 1792 + lId], ofs + 1792 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 2048 + lId], ofs + 2048 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2304 + lId], ofs + 2304 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2560 + lId], ofs + 2560 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 2816 + lId], ofs + 2816 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 3072 + lId], ofs + 3072 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3328 + lId], ofs + 3328 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3584 + lId], ofs + 3584 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 3840 + lId], ofs + 3840 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 4096 + lId], ofs + 4096 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4352 + lId], ofs + 4352 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4608 + lId], ofs + 4608 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 4864 + lId], ofs + 4864 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 5120 + lId], ofs + 5120 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5376 + lId], ofs + 5376 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5632 + lId], ofs + 5632 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 5888 + lId], ofs + 5888 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 6144 + lId], ofs + 6144 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6400 + lId], ofs + 6400 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6656 + lId], ofs + 6656 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 6912 + lId], ofs + 6912 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	masking4(input0[ofs + 7168 + lId], ofs + 7168 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	masking4(input0[ofs + 7424 + lId], ofs + 7424 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7680) < iCNT[0]) masking4(input0[ofs + 7680 + lId], ofs + 7680 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 7936) < iCNT[0]) masking4(input0[ofs + 7936 + lId], ofs + 7936 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	if ((lId + 8192) < iCNT[0]) masking4(input0[ofs + 8192 + lId], ofs + 8192 + lId, &scratch[0], &tab[0], &iCNT[1], mask);
	if ((lId + 8448) < iCNT[0]) masking4(input0[ofs + 8448 + lId], ofs + 8448 + lId, &scratch[0], &tab[0], &iCNT[1], mask);

	__syncthreads();

	u32 inLim = min(iCNT[1], (u32)1216);

	__syncthreads();

	u32 ownPos = lId;
	u32 own = scratch4[ownPos];
	u32 othPos = own;
	u32 buck, pos;
	u32 cnt = 0;

	uint2 outputEl;

	while (ownPos < inLim) {
		u32 addr = (othPos < inLim) ? othPos : ownPos + 256;
		u32 elem = scratch4[addr];

		if (othPos < inLim) {
			outputEl.x = scratch0[ownPos] ^ scratch0[othPos];
			outputEl.y = scratch1[ownPos] ^ scratch1[othPos];
			if ((outputEl.x == 0) && (outputEl.y == 0)) {			// Last round we want all bits to vanish
				uint4 index;
				index.x = scratch2[ownPos];
				index.y = scratch3[ownPos];
				index.z = scratch2[othPos];
				index.w = scratch3[othPos];

				bool ok = true;
				ok = ok && (index.x != index.y) && (index.x != index.z) && (index.x != index.w);
				ok = ok && (index.y != index.z) && (index.y != index.w) && (index.z != index.w);

				if (ok) {
					pos = atomicAdd(&outCounter[0], 1);
					if (pos < 256) {
						output0[pos] = index;
					}
				}
			}
		}
		else {
			own = elem;
			ownPos += 256;
		}

		othPos = elem;
		ownPos = (cnt<40) ? ownPos : inLim;
		cnt++;
	}
}

__global__ void combine(
	uint4 * inputR2,
	uint4 * inputR3,
	uint4 * inputR4,
	uint2 * inputR1,
	uint4 * inputR5,
	u32 * counters,
	uint4 * results)
{
	const u32 lId = threadIdx.x;
	const u32 gId = blockIdx.x;

	u32 * inCounter = &counters[40960];
	u32 * outCounters = (u32*) &results[0];

	__shared__ u32 scratch0[32];
	__shared__ u32 scratch1[32];
	__shared__ u32 ok[1];

	if (gId < inCounter[0]) {
		if (lId == 0) {
			ok[0] = 0;

			uint4 tmp;
			tmp = inputR5[gId];

			scratch1[4 * lId + 0] = tmp.x;
			scratch1[4 * lId + 1] = tmp.y;
			scratch1[4 * lId + 2] = tmp.z;
			scratch1[4 * lId + 3] = tmp.w;
		}

		__syncthreads();

		if (lId < 4) {								// Read the output of Round 3
			u32 addr = scratch1[lId];
			if (addr < 71041024) {
				uint4 tmp = inputR3[addr];

				scratch0[2 * lId] = tmp.z;
				scratch0[2 * lId + 1] = tmp.w;
			}
		}

		__syncthreads();

		if (lId < 8) {								// Read the output of Round 2
			u32 addr = scratch0[lId];
			if (addr < 71041024) {
				uint4 tmp = inputR2[addr];

				tmp.x = tmp.w & 0x3FFF;				// Unpack the representation
				tmp.y = (tmp.w >> 14) & 0x3FFF;

				tmp.z = tmp.z >> 23;
				tmp.w = tmp.w >> 28;

				tmp.w |= (tmp.z << 4);
				tmp.w *= 8672;

				scratch1[2 * lId] = tmp.x + tmp.w;
				scratch1[2 * lId + 1] = tmp.y + tmp.w;
			}
		}

		__syncthreads();

		if (lId < 16) {								// Read the output of Round 1
			u32 addr = scratch1[lId];
			if (addr < 71041024) {
				uint2 tmp = inputR1[addr];

				scratch0[2 * lId] = tmp.x;
				scratch0[2 * lId + 1] = tmp.y;
			}
		}

		__syncthreads();

		// Check for doublicate entries

		for (u32 i = 0; i<32; i++) {
			if (scratch0[2 * lId] == scratch0[i])   atomicAdd(&ok[0], 1);
			if (scratch0[2 * lId + 1] == scratch0[i]) atomicAdd(&ok[0], 1);
		}

		__syncthreads();

		if (ok[0] == 32) {						// Only entry to itself may be equal
			u32 addr;
			if (lId == 0) addr = atomicAdd(&outCounters[0], 1);

			uint2 elem;
			elem.x = scratch0[2 * lId];
			elem.y = scratch0[2 * lId + 1];

			if (elem.x > elem.y) {
				u32 t = elem.x;
				elem.x = elem.y;
				elem.y = t;
			}

			scratch1[2 * lId] = elem.x;
			scratch1[2 * lId + 1] = elem.y;				// Elements sorted by 2 Elem

			__syncthreads();
			// Do the Equihash element sorting

			uint2 tmp2;

			tmp2.x = lId >> 1;
			tmp2.y = (scratch1[4 * tmp2.x + 0] > scratch1[4 * tmp2.x + 2]) ? (lId ^ 0x1) : lId;

			scratch0[2 * lId] = scratch1[2 * tmp2.y];
			scratch0[2 * lId + 1] = scratch1[2 * tmp2.y + 1];		// Elements sorted by 4 Elem

			__syncthreads();

			tmp2.x = lId >> 2;
			tmp2.y = (scratch0[8 * tmp2.x + 0] > scratch0[8 * tmp2.x + 4]) ? (lId ^ 0x2) : lId;

			scratch1[2 * lId + 0] = scratch0[2 * tmp2.y + 0];		// Elements sorted by 8 Elem
			scratch1[2 * lId + 1] = scratch0[2 * tmp2.y + 1];

			__syncthreads();

			tmp2.x = lId >> 3;
			tmp2.y = (scratch1[16 * tmp2.x + 0] > scratch1[16 * tmp2.x + 8]) ? (lId ^ 0x4) : lId;

			scratch0[2 * lId + 0] = scratch1[2 * tmp2.y + 0];		// Elements sorted by 16 Elem
			scratch0[2 * lId + 1] = scratch1[2 * tmp2.y + 1];

			__syncthreads();

			tmp2.x = lId >> 4;
			tmp2.y = (scratch0[32 * tmp2.x + 0] > scratch0[32 * tmp2.x + 16]) ? (lId ^ 0x8) : lId;

			scratch1[2 * lId + 0] = scratch0[2 * tmp2.y + 0];		// Elements sorted by 32 Elem
			scratch1[2 * lId + 1] = scratch0[2 * tmp2.y + 1];

			__syncthreads();
			// All Elements sorted	

			if (lId == 0) scratch0[0] = addr;

			__syncthreads();

			addr = scratch0[0];

			if ((addr < 10) && (lId < 8)) {
				uint4 tmp;
				tmp.x = scratch1[4 * lId];
				tmp.y = scratch1[4 * lId + 1];
				tmp.z = scratch1[4 * lId + 2];
				tmp.w = scratch1[4 * lId + 3];

				results[1 + 8 * addr + lId] = tmp;
			}
		}
	}
}

__host__ CudaSolver::CudaSolver(const std::string &aId, int aDeviceId)
	: _id(aId)
	, _deviceId(aDeviceId)
{
	const size_t unit0Size = sizeof(uint4) * 71303168;
	const size_t unit1Size = sizeof(uint4) * 71303168;
	const size_t unit2Size = sizeof(uint4) * 71303168;
	const size_t unit3Size = sizeof(uint2) * 71303168;
	const size_t unit4Size = sizeof(uint4) * 256;
	const size_t unit5Size = sizeof(u32) * 49152;
	const size_t unit6Size = sizeof(u32) * 324;
	const size_t memorySize = unit0Size + unit1Size + unit2Size + unit3Size + unit4Size + unit5Size + unit6Size;
	
	ThrowIfCudaErrors(cudaGetDeviceProperties(&_deviceProps, _deviceId));

	if (_deviceProps.totalGlobalMem < (memorySize)) {
		LOG(Error) << "CUDA device " << std::string(_deviceProps.name) << " has insufficient GPU memory." << _deviceProps.totalGlobalMem << " bytes of memory found < " << (memorySize) << " bytes of memory required";
		throw std::runtime_error("CUDA: failed to init");
	}

	ThrowIfCudaErrors(cudaSetDevice(_deviceId));
	ThrowIfCudaErrors(cudaDeviceReset());
	ThrowIfCudaErrors(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync/* | cudaDeviceMapHost*/));
	ThrowIfCudaErrors(cudaDeviceSetCacheConfig(cudaFuncCachePreferShared));

	if (cudaHostAlloc(&_solutions, sizeof(uint4) * 81, cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit0, unit0Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit1, unit1Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit2, unit2Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit3, unit3Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit4, unit4Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit5, unit5Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.unit6, unit6Size) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}
}

#define	DO_METRICS	0

__host__ void CudaSolver::Solve(BeamWork::Ref aWork, Listener &aListener)
{
#if DO_METRICS
	std::chrono::time_point<std::chrono::system_clock>	_start = std::chrono::system_clock::now();
	uint64_t cycleStart = __rdtsc();
#endif
#if DO_METRICS
	uint64_t cycleSetHeader = __rdtsc();
#endif
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleCopyBlake = __rdtsc();
#endif
	ThrowIfCudaErrors(cudaMemset(_memory.unit5, 0, sizeof(u32) * 49152));
	ThrowIfCudaErrors(cudaMemset(_memory.unit6, 0, sizeof(uint4) * 81));
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleMemset = __rdtsc();
#endif
	ulonglong4 blockHeader;
	blockHeader.x = reinterpret_cast<ulonglong4*>(aWork->GetData())->x;
	blockHeader.y = reinterpret_cast<ulonglong4*>(aWork->GetData())->y;
	blockHeader.z = reinterpret_cast<ulonglong4*>(aWork->GetData())->z;
	blockHeader.w = reinterpret_cast<ulonglong4*>(aWork->GetData())->w;
	round_init <<< 22369536/256, 256 >>>(_memory.unit0, (uint2*)_memory.unit2, _memory.unit5, blockHeader.x, blockHeader.y, blockHeader.z, blockHeader.w, aWork->_nonce);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_0 = __rdtsc();
#endif
	round_1 <<< 16777216/256, 256 >>>(_memory.unit0, (uint2*)_memory.unit2, _memory.unit1, _memory.unit3, _memory.unit5);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_1 = __rdtsc();
#endif
	round_2 <<< 16777216/256, 256 >>>(_memory.unit1, _memory.unit0, _memory.unit5);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_2 = __rdtsc();
#endif
	round_3 <<< 16777216/256, 256 >>>(_memory.unit0, _memory.unit1, _memory.unit5);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_3 = __rdtsc();
#endif
	round_4 <<< 16777216/256, 256 >>>(_memory.unit1, _memory.unit2, _memory.unit5);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_4 = __rdtsc();
#endif
	round_5 <<< 16777216/256, 256 >>>(_memory.unit2, _memory.unit4, _memory.unit5);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_5 = __rdtsc();
#endif
	combine <<< 4096/16, 16 >> >(_memory.unit0, _memory.unit1, _memory.unit2, _memory.unit3, _memory.unit4, _memory.unit5, (uint4*)_memory.unit6);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_6 = __rdtsc();
#endif
#if DEEP_CUDA_DEBUG
	ThrowIfCudaErrors(cudaMemcpy(_hostEq, _deviceEq, sizeof(equi), cudaMemcpyDeviceToHost));
	u32 m[4] = { 0, 0, 0, 0 };
	for (u32 i = 0; i < Equihash::BucketsCount; i++) {
		if (_hostEq->edata.nslots[0][i] > m[0]) {
			m[0] = _hostEq->edata.nslots[0][i];
		}
		if (_hostEq->edata.nslots[1][i] > m[1]) {
			m[1] = _hostEq->edata.nslots[1][i];
		}
		if (_hostEq->edata.nslots[2][i] > m[2]) {
			m[2] = _hostEq->edata.nslots[2][i];
		}
		if (_hostEq->edata.nslots[3][i] > m[3]) {
			m[3] = _hostEq->edata.nslots[3][i];
		}
	}
	scontainerreal *_solutions = &_hostEq->edata.srealcont;
#else
	ThrowIfCudaErrors(cudaMemcpy(_solutions, _memory.unit6, sizeof(uint4) * 81, cudaMemcpyDeviceToHost));
#endif
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleCopySol = __rdtsc();

	double cycleTotal = cycleCopySol - cycleStart;

	LOG(Info) << "cycleSetHeader = " << (cycleSetHeader - cycleStart) << " (" << ((cycleSetHeader - cycleStart) / cycleTotal) << ")";
	LOG(Info) << "cycleCopyBlake = " << (cycleCopyBlake - cycleSetHeader) << " (" << ((cycleCopyBlake - cycleSetHeader) / cycleTotal) << ")";
	LOG(Info) << "cycleMemset = " << (cycleMemset - cycleCopyBlake) << " (" << ((cycleMemset - cycleCopyBlake) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_0 = " << (cycleDigit_0 - cycleMemset) << " (" << ((cycleDigit_0 - cycleMemset) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_1 = " << (cycleDigit_1 - cycleDigit_0) << " (" << ((cycleDigit_1 - cycleDigit_0) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_2 = " << (cycleDigit_2 - cycleDigit_1) << " (" << ((cycleDigit_2 - cycleDigit_1) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_3 = " << (cycleDigit_3 - cycleDigit_2) << " (" << ((cycleDigit_3 - cycleDigit_2) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_4 = " << (cycleDigit_4 - cycleDigit_3) << " (" << ((cycleDigit_4 - cycleDigit_3) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_5 = " << (cycleDigit_5 - cycleDigit_4) << " (" << ((cycleDigit_5 - cycleDigit_4) / cycleTotal) << ")";
	LOG(Info) << "cycleCombine = " << (cycleDigit_6 - cycleDigit_5) << " (" << ((cycleDigit_6 - cycleDigit_5) / cycleTotal) << ")";
	LOG(Info) << "cycleCopySol = " << (cycleCopySol - cycleDigit_6) << " (" << ((cycleCopySol - cycleDigit_6) / cycleTotal) << ")";
	LOG(Info) << "cycleTotal = " << (uint64_t)cycleTotal;
	LOG(Info) << "time = " << (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count() << " ms";
	LOG(Info) << "Solutions count: " << _solutions[0];
#endif
	// Read the number of solutions of the last iteration
	uint32_t solutions = _solutions[0];
	for (uint32_t i = 0; i < solutions; i++) {
		std::vector<uint32_t> indexes;
		indexes.assign(32, 0);
		memcpy(indexes.data(), &_solutions[4 + 32 * i], sizeof(uint32_t) * 32);

		aListener.OnSolution(*aWork, indexes, 25);
	}

	aListener.OnHashDone();
}

__host__ CudaSolver::~CudaSolver()
{
	if (_solutions) {
		cudaFreeHost(_solutions);
		_solutions = nullptr;
	}

	if (_memory.unit0) {
		cudaFree(_memory.unit0);
		_memory.unit0 = nullptr;
	}

	if (_memory.unit1) {
		cudaFree(_memory.unit1);
		_memory.unit1 = nullptr;
	}

	if (_memory.unit2) {
		cudaFree(_memory.unit2);
		_memory.unit2 = nullptr;
	}

	if (_memory.unit3) {
		cudaFree(_memory.unit3);
		_memory.unit3 = nullptr;
	}

	if (_memory.unit4) {
		cudaFree(_memory.unit4);
		_memory.unit4 = nullptr;
	}

	if (_memory.unit5) {
		cudaFree(_memory.unit5);
		_memory.unit5 = nullptr;
	}

	if (_memory.unit6) {
		cudaFree(_memory.unit6);
		_memory.unit6 = nullptr;
	}

	cudaDeviceReset();
}

