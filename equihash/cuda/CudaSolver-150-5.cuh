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

#define	BEAM_WORK_MODE	1

#if BEAM_WORK_MODE
#include "blake2b.cuh"
#endif

struct Equihash
{
	const static u32 DigitsCount	= (WK + 1);
	const static u32 DigitBits		= (WN / (DigitsCount));
	const static u32 RestBits		= 9;
	const static u32 HashOutput		= (512 / WN) * ((WN + 7) / 8);
	const static u32 BucketBits		= (DigitBits - RestBits);
	const static u32 BucketsCount	= (1 << BucketBits);
	const static u32 BucketsMask	= (BucketsCount - 1);
	const static u32 RestsCount		= (1 << RestBits);
	const static u32 RestMask		= (RestsCount - 1);
	const static u32 ItemsCount		= (2 << DigitBits);
#if 0
	const static u32 SlotsCount		= (2 * RestsCount);
	const static u32 InitSlotsCount = (SlotsCount + 256);
#else
	const static u32 SlotsCount		= (2 * RestsCount + 256);
#endif
//	const static u32 SlotsMask		= (SlotsCount - 1);
	const static u32 ItemsPerBlake	= (512 / WN);
	const static u32 BlakesCount	= ((ItemsCount + ItemsPerBlake - 1) / ItemsPerBlake);

	const static size_t MemoryUnitSize32	= (BucketsCount * SlotsCount);
	const static size_t MemoryUnitsCount	= 13;
};

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

typedef u32 proof[PROOFSIZE];

typedef u32 InitSlot[5];
typedef u32 Slot3[3];
typedef u32 (*BaseMap)[Equihash::SlotsCount];
typedef InitSlot (*Items)[Equihash::SlotsCount];
typedef uint4 (*Items4)[Equihash::SlotsCount];
typedef Slot3 (*Items3)[Equihash::SlotsCount];
typedef uint2 (*Items2)[Equihash::SlotsCount];
typedef uint2 (*Pairs)[Equihash::SlotsCount];

struct equi
{
#if BEAM_WORK_MODE
	blake2b_state blake_ctx;
#else
	union
	{
		u64 blake_h[8];
		u32 blake_h32[16];
	};
#endif
	struct
	{
		u32 nslots[5][Equihash::BucketsCount];
		scontainerreal srealcont;
		u32 outOfRange;
		u32 indexErrorJ;
		u32 indexErrorK;
	} edata;
};
#if !BEAM_WORK_MODE
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
#endif
__global__ void stage_init(equi *eq, BaseMap baseMap, Items xorOut)
{
	const u32 block = blockIdx.x * blockDim.x + threadIdx.x;
#if BEAM_WORK_MODE
	blake2b_state state;
#else
	__shared__ u64 hash_h[8];
	u32* hash_h32 = (u32*)hash_h;

	if (threadIdx.x < 16) {
		hash_h32[threadIdx.x] = __ldca(&eq->blake_h32[threadIdx.x]);
	}

	__syncthreads();
#endif
	if (block < Equihash::BlakesCount)
	{
#if BEAM_WORK_MODE
		state = eq->blake_ctx;
		blake2b_gpu_hash(&state, block);
		u32 *v32 = (u32*)&state.h[0];
#else
		u64 m = (u64)block << 32;

		union
		{
			u64 v[16];
			u32 v32[32];
			uint4 v128[8];
		};

		v[0] = hash_h[0];
		v[1] = hash_h[1];
		v[2] = hash_h[2];
		v[3] = hash_h[3];
		v[4] = hash_h[4];
		v[5] = hash_h[5];
		v[6] = hash_h[6];
		v[7] = hash_h[7];
		v[8] = blake_iv[0];
		v[9] = blake_iv[1];
		v[10] = blake_iv[2];
		v[11] = blake_iv[3];
		v[12] = blake_iv[4] ^ (128 + 16);
		v[13] = blake_iv[5];
		v[14] = blake_iv[6] ^ 0xffffffffffffffff;
		v[15] = blake_iv[7];

		// mix 1
		G2(v[0], v[4], v[8], v[12], 0, m);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 2
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], m, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 3
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, m);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 4
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, m);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 5
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, m);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 6
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], m, 0);

		// mix 7
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], m, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 8
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, m);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 9
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], m, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 10
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], m, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 11
		G2(v[0], v[4], v[8], v[12], 0, m);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], 0, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		// mix 12
		G2(v[0], v[4], v[8], v[12], 0, 0);
		G2(v[1], v[5], v[9], v[13], 0, 0);
		G2(v[2], v[6], v[10], v[14], 0, 0);
		G2(v[3], v[7], v[11], v[15], 0, 0);
		G2(v[0], v[5], v[10], v[15], m, 0);
		G2(v[1], v[6], v[11], v[12], 0, 0);
		G2(v[2], v[7], v[8], v[13], 0, 0);
		G2(v[3], v[4], v[9], v[14], 0, 0);

		v[0] ^= hash_h[0] ^ v[8];
		v[1] ^= hash_h[1] ^ v[9];
		v[2] ^= hash_h[2] ^ v[10];
		v[3] ^= hash_h[3] ^ v[11];
		v[4] ^= hash_h[4] ^ v[12];
		v[5] ^= hash_h[5] ^ v[13];
		v[6] ^= hash_h[6] ^ v[14];
		v[7] ^= hash_h[7] ^ v[15];
#endif
		/*
		blake	0	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16	17	18	19	20	21	22	23	24	25	26	27	28	29	30	31	32	33	34	35	36	37	38	39	40	41	42	43	44	45	46	47	48	49	50	51	52	53	54	55	56
		words	0	0	0	0	1	1	1	1	2	2	2	2	3	3	3	3	4	4	4	4	5	5	5	5	6	6	6	6	7	7	7	7	8	8	8	8	9	9	9	9	10	10	10	10	11	11	11	11	12	12	12	12	13	13	13	13	14
		hash	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	2	2	2	2	2	2	2	2	2	2	2	2	2	2	2	2	2	2	2

		0	0	0	1	1	1	1	2	2	2	2	3	3	3	3	4	4	4	4	0	0	0	1	1	1	1	2	2	2	2	3	3	3	3	4	4	4	4	0	0	0	1	1	1	1	2	2	2	2	3	3	3	3	4	4	4	4
		*/
		u32 slotp;
		u32 w;
		u32 bucketid;

		u32 itemIndex = Equihash::ItemsPerBlake * block;
		if (itemIndex < Equihash::ItemsCount)
		{
			w = __byte_perm(v32[0], 0, 0x0123); // first 32 bits
			asm("bfe.u32 %0, %1, 16, 16;" : "=r"(bucketid) : "r"(w));
			slotp = atomicAdd(&eq->edata.nslots[0][bucketid], 1);
			if (slotp < Equihash::SlotsCount)
			{
				baseMap[bucketid][slotp] = itemIndex;

				w = __byte_perm(w, 0, 0x4510) >> 7;
				xorOut[bucketid][slotp][0] = w;
				w = __byte_perm(v32[0], v32[1], 0x3456) & 0x7fffffff;
				xorOut[bucketid][slotp][1] = w;
				w = __byte_perm(v32[1], v32[2], 0x3456);
				xorOut[bucketid][slotp][2] = w;
				w = __byte_perm(v32[2], v32[3], 0x3456);
				xorOut[bucketid][slotp][3] = w;
				w = __byte_perm(v32[3], v32[4], 0x3456) & 0xfffffffc;
				xorOut[bucketid][slotp][4] = w;
			}
		}

		itemIndex++;
		if (itemIndex < Equihash::ItemsCount)
		{
			w = __byte_perm(v32[4], v32[5], 0x3456); // first 32 bits
			asm("bfe.u32 %0, %1, 16, 16;" : "=r"(bucketid) : "r"(w));
			slotp = atomicAdd(&eq->edata.nslots[0][bucketid], 1);
			if (slotp < Equihash::SlotsCount)
			{
				baseMap[bucketid][slotp] = itemIndex;

				w = __byte_perm(w, 0, 0x4510) >> 7;
				xorOut[bucketid][slotp][0] = w;
				w = __byte_perm(v32[5], v32[6], 0x2345) & 0x7fffffff;
				xorOut[bucketid][slotp][1] = w;
				w = __byte_perm(v32[6], v32[7], 0x2345);
				xorOut[bucketid][slotp][2] = w;
				w = __byte_perm(v32[7], v32[8], 0x2345);
				xorOut[bucketid][slotp][3] = w;
				w = __byte_perm(v32[8], v32[9], 0x2345) & 0xfffffffc;
				xorOut[bucketid][slotp][4] = w;
			}
		}

		itemIndex++;
		if (itemIndex < Equihash::ItemsCount)
		{
			w = __byte_perm(v32[9], v32[10], 0x2345); // first 32 bits
			asm("bfe.u32 %0, %1, 16, 16;" : "=r"(bucketid) : "r"(w));
			slotp = atomicAdd(&eq->edata.nslots[0][bucketid], 1);
			if (slotp < Equihash::SlotsCount)
			{
				baseMap[bucketid][slotp] = itemIndex;

				w = __byte_perm(w, 0, 0x4510) >> 7;
				xorOut[bucketid][slotp][0] = w;
				w = __byte_perm(v32[10], v32[11], 0x1234) & 0x7fffffff;
				xorOut[bucketid][slotp][1] = w;
				w = __byte_perm(v32[11], v32[12], 0x1234);
				xorOut[bucketid][slotp][2] = w;
				w = __byte_perm(v32[12], v32[13], 0x1234);
				xorOut[bucketid][slotp][3] = w;
				w = __byte_perm(v32[13], v32[14], 0x1234) & 0xfffffffc;
				xorOut[bucketid][slotp][4] = w;
#if 0
				uint4 ta;

				ta.x = __byte_perm(v32[10], v32[11], 0x1234) & 0x7fffffff;
				ta.y = __byte_perm(v32[11], v32[12], 0x1234);
				ta.z = __byte_perm(v32[12], v32[13], 0x1234);
				ta.w = __byte_perm(v32[13], v32[14], 0x1234);

				((uint4*)xorOut)[index] = ta;
#endif
			}
		}
	}
}

template <u32 SSM>
__global__ void stage_1(equi *eq, Items aInXor, Items4 aOutXor, Pairs aPairs)
{
	__shared__ uint4 cache[Equihash::SlotsCount];
	__shared__ u32 ht_len[Equihash::RestsCount];
	__shared__ u16 ht[Equihash::RestsCount][SSM - 1];

	const u32 threadid = threadIdx.x;
	const u32 bucketid = blockIdx.x;

	ht_len[threadid] = 0;
	ht_len[256 + threadid] = 0;

	u32 backetSize = umin(eq->edata.nslots[0][bucketid], Equihash::SlotsCount);

	u32 slotIndex;
	u32 hashIndex;
	u32 pos;
	uint4 tb;

	__syncthreads();

#pragma unroll
	for (u32 i = 0; i < 5; ++i)
	{
		slotIndex = i * 256 + threadid;
		if (slotIndex >= backetSize) {
			break;
		}

		hashIndex = aInXor[bucketid][slotIndex][0];
		tb.x = aInXor[bucketid][slotIndex][1];
		tb.y = aInXor[bucketid][slotIndex][2];
		tb.z = aInXor[bucketid][slotIndex][3];
		tb.w = aInXor[bucketid][slotIndex][4];

		cache[slotIndex] = tb;

		pos = atomicAdd(&ht_len[hashIndex], 1);
		if (pos < (SSM - 1)) {
			ht[hashIndex][pos] = slotIndex;
		}
	}

	__syncthreads();

	uint4 xors;
	uint2 ps;
	u32 xorbucketid, xorslot;

#pragma unroll
	for (u32 i = 0; i < 2; i++) {
		u32 htIndex = i * 256 + threadid;
		if (ht_len[htIndex] > 1) {
			u32 length = umin(ht_len[htIndex], (SSM - 1));
			for (u32 j = 1; j < length; j++) {
				u32 ij = ht[htIndex][j];
				uint4 ta = cache[ij];
				for (u32 k = 0; k < j; k++) {
					u32 ik = ht[htIndex][k];

					xors.x = ta.x ^ cache[ik].x;
					xors.y = ta.y ^ cache[ik].y;
					xors.z = ta.z ^ cache[ik].z;
					xors.w = ta.w ^ cache[ik].w;

					asm("bfe.u32 %0, %1, 15, 16;" : "=r"(xorbucketid) : "r"(xors.x));
					xorslot = atomicAdd(&eq->edata.nslots[1][xorbucketid], 1);

					if (xorslot < Equihash::SlotsCount)
					{
						aOutXor[xorbucketid][xorslot] = xors;
						ps.x = __byte_perm(bucketid, ij, 0x1054);
						ps.y = __byte_perm(bucketid, ik, 0x1054);
						aPairs[xorbucketid][xorslot] = ps;
					}
				}
			}
		}
	}
}

template <u32 SSM>
__global__ void stage_2(equi *eq, const Items4 aInXor, Items3 aOutXor, Pairs aPairs)
{
	__shared__ uint4 cache[Equihash::SlotsCount];
	__shared__ u32 ht_len[Equihash::RestsCount];
	__shared__ u16 ht[Equihash::RestsCount][SSM - 1];

	const u32 threadid = threadIdx.x;
	const u32 bucketid = blockIdx.x;

	ht_len[threadid] = 0;
	ht_len[256 + threadid] = 0;

	u32 backetSize = umin(eq->edata.nslots[1][bucketid], Equihash::SlotsCount);

	u32 slotIndex;
	u32 hashIndex;
	u32 pos;
	uint4 ta;

	__syncthreads();

#pragma unroll
	for (u32 i = 0; i < 5; ++i)
	{
		slotIndex = i * 256 + threadid;
		if (slotIndex >= backetSize) {
			break;
		}

		cache[slotIndex] = ta = aInXor[bucketid][slotIndex];

		asm("bfe.u32 %0, %1, 6, 9;" : "=r"(hashIndex) : "r"(ta.x));

		pos = atomicAdd(&ht_len[hashIndex], 1);
		if (pos < (SSM - 1)) {
			ht[hashIndex][pos] = slotIndex;
		}
	}

	__syncthreads();

	uint4 xors;
	uint2 ps;
	u32 xorbucketid, xorslot;

#pragma unroll
	for (u32 i = 0; i < 2; i++) {
		u32 htIndex = i * 256 + threadid;
		if (ht_len[htIndex] > 1) {
			u32 length = umin(ht_len[htIndex], (SSM - 1));
			for (u32 j = 1; j < length; j++) {
				u32 ij = ht[htIndex][j];
				ta = cache[ij];
				for (u32 k = 0; k < j; k++) {
					u32 ik = ht[htIndex][k];

					xors.x = ta.x ^ cache[ik].x;
					xors.y = ta.y ^ cache[ik].y;
					xors.z = ta.z ^ cache[ik].z;
					xors.w = ta.w ^ cache[ik].w;

					xorbucketid = __byte_perm(xors.x, xors.y, 0x0765);
					asm("bfe.u32 %0, %1, 14, 16;" : "=r"(xorbucketid) : "r"(xorbucketid));
					xorslot = atomicAdd(&eq->edata.nslots[2][xorbucketid], 1);

					if (xorslot < Equihash::SlotsCount)
					{
						aOutXor[xorbucketid][xorslot][0] = xors.y;
						aOutXor[xorbucketid][xorslot][1] = xors.z;
						aOutXor[xorbucketid][xorslot][2] = xors.w;
						ps.x = __byte_perm(bucketid, ij, 0x1054);
						ps.y = __byte_perm(bucketid, ik, 0x1054);
						aPairs[xorbucketid][xorslot] = ps;
					}
				}
			}
		}
	}
}

template <u32 SSM>
__global__ void stage_3(equi *eq, const Items3 aInXor, Items2 aOutXor, Pairs aPairs)
{
	__shared__ uint4 cache[Equihash::SlotsCount];
	__shared__ u32 ht_len[Equihash::RestsCount];
	__shared__ u16 ht[Equihash::RestsCount][SSM - 1];

	const u32 threadid = threadIdx.x;
	const u32 bucketid = blockIdx.x;

	ht_len[threadid] = 0;
	ht_len[256 + threadid] = 0;

	u32 backetSize = umin(eq->edata.nslots[2][bucketid], Equihash::SlotsCount);

	u32 slotIndex;
	u32 hashIndex;
	u32 pos;
	uint4 ta;

	__syncthreads();

#pragma unroll
	for (u32 i = 0; i < 5; ++i)
	{
		slotIndex = i * 256 + threadid;
		if (slotIndex >= backetSize) {
			break;
		}

		cache[slotIndex].y = ta.y = aInXor[bucketid][slotIndex][0];
		cache[slotIndex].z = ta.z = aInXor[bucketid][slotIndex][1];
		cache[slotIndex].w = ta.w = aInXor[bucketid][slotIndex][2];

		asm("bfe.u32 %0, %1, 13, 9;" : "=r"(hashIndex) : "r"(ta.y));

		pos = atomicAdd(&ht_len[hashIndex], 1);
		if (pos < (SSM - 1)) {
			ht[hashIndex][pos] = slotIndex;
		}
	}

	__syncthreads();

	uint4 xors;
	uint2 ps;
	u32 xorbucketid, xorslot;

#pragma unroll
	for (u32 i = 0; i < 2; i++) {
		u32 htIndex = i * 256 + threadid;
		if (ht_len[htIndex] > 1) {
			u32 length = umin(ht_len[htIndex], (SSM - 1));
			for (u32 j = 1; j < length; j++) {
				u32 ij = ht[htIndex][j];
				ta = cache[ij];
				for (u32 k = 0; k < j; k++) {
					u32 ik = ht[htIndex][k];

					xors.x = ta.x ^ cache[ik].x;
					xors.y = ta.y ^ cache[ik].y;
					xors.z = ta.z ^ cache[ik].z;
					xors.w = ta.w ^ cache[ik].w;

					xorbucketid = __byte_perm(xors.y, xors.z, 0x1076);
					asm("bfe.u32 %0, %1, 13, 16;" : "=r"(xorbucketid) : "r"(xorbucketid));
					xorslot = atomicAdd(&eq->edata.nslots[3][xorbucketid], 1);

					if (xorslot < Equihash::SlotsCount)
					{
						ps.x = xors.z & 0x1fffffff;
						ps.y = xors.w;
						aOutXor[xorbucketid][xorslot] = ps;
						ps.x = __byte_perm(bucketid, ij, 0x1054);
						ps.y = __byte_perm(bucketid, ik, 0x1054);
						aPairs[xorbucketid][xorslot] = ps;
					}
				}
			}
		}
	}
}

template <u32 SSM>
__global__ void stage_4(equi *eq, const Items2 aInXor, Items2 aOutXor, Pairs aPairs)
{
	__shared__ uint2 cache[Equihash::SlotsCount];
	__shared__ u32 ht_len[Equihash::RestsCount];
	__shared__ u16 ht[Equihash::RestsCount][SSM - 1];

	const u32 threadid = threadIdx.x;
	const u32 bucketid = blockIdx.x;

	ht_len[threadid] = 0;
	ht_len[256 + threadid] = 0;

	u32 backetSize = umin(eq->edata.nslots[3][bucketid], Equihash::SlotsCount);

	u32 slotIndex;
	u32 hashIndex;
	u32 pos;
	uint2 ta;

	__syncthreads();

#pragma unroll
	for (u32 i = 0; i < 5; ++i)
	{
		slotIndex = i * 256 + threadid;
		if (slotIndex >= backetSize) {
			break;
		}

		cache[slotIndex] = ta = aInXor[bucketid][slotIndex];

		asm("bfe.u32 %0, %1, 20, 9;" : "=r"(hashIndex) : "r"(ta.x));

		pos = atomicAdd(&ht_len[hashIndex], 1);
		if (pos < (SSM - 1)) {
			ht[hashIndex][pos] = slotIndex;
		}
	}

	__syncthreads();

	uint2 xors;
	uint2 ps;
	u32 xorbucketid, xorslot;

#pragma unroll
	for (u32 i = 0; i < 2; i++) {
		u32 htIndex = i * 256 + threadid;
		if (ht_len[htIndex] > 1) {
			u32 length = umin(ht_len[htIndex], (SSM - 1));
			for (u32 j = 1; j < length; j++) {
				u32 ij = ht[htIndex][j];
				ta = cache[ij];
				for (u32 k = 0; k < j; k++) {
					u32 ik = ht[htIndex][k];

					xors = ta ^ cache[ik];

					asm("bfe.u32 %0, %1, 4, 16;" : "=r"(xorbucketid) : "r"(xors.x));
					xorslot = atomicAdd(&eq->edata.nslots[4][xorbucketid], 1);

					if (xorslot < Equihash::SlotsCount)
					{
						xors.x &= 0xf;
						xors.x = (xors.x << 5) | ((xors.y >> 27) & 0x1f);
						xors.y <<= 5;

						aOutXor[xorbucketid][xorslot] = xors;
						ps.x = __byte_perm(bucketid, ij, 0x1054);
						ps.y = __byte_perm(bucketid, ik, 0x1054);
						aPairs[xorbucketid][xorslot] = ps;
					}
				}
			}
		}
	}
}

template <u32 SSM>
__global__ void stage_last(equi *eq, BaseMap aBaseMap, uint2 *aPairs, Items2 aInXor)
{
	__shared__ u32 cache[Equihash::SlotsCount];
	__shared__ u32 ht_len[Equihash::RestsCount];
	__shared__ u16 ht[Equihash::RestsCount][SSM - 1];

	const u32 threadid = threadIdx.x;
	const u32 bucketid = blockIdx.x;

	ht_len[threadid] = 0;
	ht_len[256 + threadid] = 0;

	u32 backetSize = umin(eq->edata.nslots[4][bucketid], Equihash::SlotsCount);

	u32 slotIndex;
	u32 hashIndex;
	u32 pos;
	uint2 tb;

	__syncthreads();

#pragma unroll
	for (u32 i = 0; i < 5; ++i)
	{
		slotIndex = i * 256 + threadid;
		if (slotIndex >= backetSize) {
			break;
		}

		tb = aInXor[bucketid][slotIndex];
		cache[slotIndex] = tb.y;

		asm("bfe.u32 %0, %1, 0, 9;" : "=r"(hashIndex) : "r"(tb.x));

		pos = atomicAdd(&ht_len[hashIndex], 1);
		if (pos < (SSM - 1)) {
			ht[hashIndex][pos] = slotIndex;
		}
	}

	Pairs pairsTree[5];

	pairsTree[1] = (Pairs)(aPairs + 0 * Equihash::BucketsCount * Equihash::SlotsCount);
	pairsTree[2] = (Pairs)(aPairs + 1 * Equihash::BucketsCount * Equihash::SlotsCount);
	pairsTree[3] = (Pairs)(aPairs + 2 * Equihash::BucketsCount * Equihash::SlotsCount);
	pairsTree[4] = (Pairs)(aPairs + 3 * Equihash::BucketsCount * Equihash::SlotsCount);

	uint2 indexes[16];

	__syncthreads();

#pragma unroll
	for (u32 i = 0; i < 2; i++) {
		u32 htIndex = i * 256 + threadid;
		if (ht_len[htIndex] > 1) {
			u32 length = umin(ht_len[htIndex], (SSM - 1));
			for (u32 j = 1; j < length; j++) {
				u32 ij = ht[htIndex][j];
				if (u32 ta = cache[ij]) {
					for (u32 k = 0; k < j; k++) {
						u32 ik = ht[htIndex][k];
						if (ta == cache[ik]) {
							indexes[0] = pairsTree[4][bucketid][ij];

							indexes[1] = pairsTree[3][indexes[0].y >> 16][indexes[0].y & 0xffff];
							indexes[0] = pairsTree[3][indexes[0].x >> 16][indexes[0].x & 0xffff];

							indexes[3] = pairsTree[2][indexes[1].y >> 16][indexes[1].y & 0xffff];
							indexes[2] = pairsTree[2][indexes[1].x >> 16][indexes[1].x & 0xffff];
							indexes[1] = pairsTree[2][indexes[0].y >> 16][indexes[0].y & 0xffff];
							indexes[0] = pairsTree[2][indexes[0].x >> 16][indexes[0].x & 0xffff];

							indexes[7] = pairsTree[1][indexes[3].y >> 16][indexes[3].y & 0xffff];
							indexes[6] = pairsTree[1][indexes[3].x >> 16][indexes[3].x & 0xffff];
							indexes[5] = pairsTree[1][indexes[2].y >> 16][indexes[2].y & 0xffff];
							indexes[4] = pairsTree[1][indexes[2].x >> 16][indexes[2].x & 0xffff];
							indexes[3] = pairsTree[1][indexes[1].y >> 16][indexes[1].y & 0xffff];
							indexes[2] = pairsTree[1][indexes[1].x >> 16][indexes[1].x & 0xffff];
							indexes[1] = pairsTree[1][indexes[0].y >> 16][indexes[0].y & 0xffff];
							indexes[0] = pairsTree[1][indexes[0].x >> 16][indexes[0].x & 0xffff];

							indexes[8] = pairsTree[4][bucketid][ik];

							indexes[9] = pairsTree[3][indexes[8].y >> 16][indexes[8].y & 0xffff];
							indexes[8] = pairsTree[3][indexes[8].x >> 16][indexes[8].x & 0xffff];

							indexes[11] = pairsTree[2][indexes[9].y >> 16][indexes[9].y & 0xffff];
							indexes[10] = pairsTree[2][indexes[9].x >> 16][indexes[9].x & 0xffff];
							indexes[9] = pairsTree[2][indexes[8].y >> 16][indexes[8].y & 0xffff];
							indexes[8] = pairsTree[2][indexes[8].x >> 16][indexes[8].x & 0xffff];

							indexes[15] = pairsTree[1][indexes[11].y >> 16][indexes[11].y & 0xffff];
							indexes[14] = pairsTree[1][indexes[11].x >> 16][indexes[11].x & 0xffff];
							indexes[13] = pairsTree[1][indexes[10].y >> 16][indexes[10].y & 0xffff];
							indexes[12] = pairsTree[1][indexes[10].x >> 16][indexes[10].x & 0xffff];
							indexes[11] = pairsTree[1][indexes[9].y >> 16][indexes[9].y & 0xffff];
							indexes[10] = pairsTree[1][indexes[9].x >> 16][indexes[9].x & 0xffff];
							indexes[9] = pairsTree[1][indexes[8].y >> 16][indexes[8].y & 0xffff];
							indexes[8] = pairsTree[1][indexes[8].x >> 16][indexes[8].x & 0xffff];

							u32 soli = atomicAdd(&eq->edata.srealcont.nsols, 1);
#if 1
							if (soli < MAXREALSOLS)
							{
								eq->edata.srealcont.sols[soli][0] = aBaseMap[indexes[0].x >> 16][indexes[0].x & 0xffff];
								eq->edata.srealcont.sols[soli][1] = aBaseMap[indexes[0].y >> 16][indexes[0].y & 0xffff];
								eq->edata.srealcont.sols[soli][2] = aBaseMap[indexes[1].x >> 16][indexes[1].x & 0xffff];
								eq->edata.srealcont.sols[soli][3] = aBaseMap[indexes[1].y >> 16][indexes[1].y & 0xffff];
								eq->edata.srealcont.sols[soli][4] = aBaseMap[indexes[2].x >> 16][indexes[2].x & 0xffff];
								eq->edata.srealcont.sols[soli][5] = aBaseMap[indexes[2].y >> 16][indexes[2].y & 0xffff];
								eq->edata.srealcont.sols[soli][6] = aBaseMap[indexes[3].x >> 16][indexes[3].x & 0xffff];
								eq->edata.srealcont.sols[soli][7] = aBaseMap[indexes[3].y >> 16][indexes[3].y & 0xffff];
								eq->edata.srealcont.sols[soli][8] = aBaseMap[indexes[4].x >> 16][indexes[4].x & 0xffff];
								eq->edata.srealcont.sols[soli][9] = aBaseMap[indexes[4].y >> 16][indexes[4].y & 0xffff];
								eq->edata.srealcont.sols[soli][10] = aBaseMap[indexes[5].x >> 16][indexes[5].x & 0xffff];
								eq->edata.srealcont.sols[soli][11] = aBaseMap[indexes[5].y >> 16][indexes[5].y & 0xffff];
								eq->edata.srealcont.sols[soli][12] = aBaseMap[indexes[6].x >> 16][indexes[6].x & 0xffff];
								eq->edata.srealcont.sols[soli][13] = aBaseMap[indexes[6].y >> 16][indexes[6].y & 0xffff];
								eq->edata.srealcont.sols[soli][14] = aBaseMap[indexes[7].x >> 16][indexes[7].x & 0xffff];
								eq->edata.srealcont.sols[soli][15] = aBaseMap[indexes[7].y >> 16][indexes[7].y & 0xffff];
								eq->edata.srealcont.sols[soli][16] = aBaseMap[indexes[8].x >> 16][indexes[8].x & 0xffff];
								eq->edata.srealcont.sols[soli][17] = aBaseMap[indexes[8].y >> 16][indexes[8].y & 0xffff];
								eq->edata.srealcont.sols[soli][18] = aBaseMap[indexes[9].x >> 16][indexes[9].x & 0xffff];
								eq->edata.srealcont.sols[soli][19] = aBaseMap[indexes[9].y >> 16][indexes[9].y & 0xffff];
								eq->edata.srealcont.sols[soli][20] = aBaseMap[indexes[10].x >> 16][indexes[10].x & 0xffff];
								eq->edata.srealcont.sols[soli][21] = aBaseMap[indexes[10].y >> 16][indexes[10].y & 0xffff];
								eq->edata.srealcont.sols[soli][22] = aBaseMap[indexes[11].x >> 16][indexes[11].x & 0xffff];
								eq->edata.srealcont.sols[soli][23] = aBaseMap[indexes[11].y >> 16][indexes[11].y & 0xffff];
								eq->edata.srealcont.sols[soli][24] = aBaseMap[indexes[12].x >> 16][indexes[12].x & 0xffff];
								eq->edata.srealcont.sols[soli][25] = aBaseMap[indexes[12].y >> 16][indexes[12].y & 0xffff];
								eq->edata.srealcont.sols[soli][26] = aBaseMap[indexes[13].x >> 16][indexes[13].x & 0xffff];
								eq->edata.srealcont.sols[soli][27] = aBaseMap[indexes[13].y >> 16][indexes[13].y & 0xffff];
								eq->edata.srealcont.sols[soli][28] = aBaseMap[indexes[14].x >> 16][indexes[14].x & 0xffff];
								eq->edata.srealcont.sols[soli][29] = aBaseMap[indexes[14].y >> 16][indexes[14].y & 0xffff];
								eq->edata.srealcont.sols[soli][30] = aBaseMap[indexes[15].x >> 16][indexes[15].x & 0xffff];
								eq->edata.srealcont.sols[soli][31] = aBaseMap[indexes[15].y >> 16][indexes[15].y & 0xffff];
							}
#endif
						}
					}
				}
			}
		}
	}
}

std::mutex dev_init;
int dev_init_done[8] = { 0 };

__host__ int compu32(const void *pa, const void *pb)
{
	uint32_t a = *(uint32_t *)pa, b = *(uint32_t *)pb;
	return a<b ? -1 : a == b ? 0 : +1;
}

__host__ bool duped(uint32_t* prf)
{
	uint32_t sortprf[PROOFSIZE];
	memcpy(sortprf, prf, sizeof(uint32_t) * PROOFSIZE);
	qsort(sortprf, PROOFSIZE, sizeof(uint32_t), &compu32);
	for (uint32_t i = 1; i < PROOFSIZE; i++) {
		if (sortprf[i] <= sortprf[i - 1]) {
			return true;
		}
	}
	return false;
}

__host__ void sort_pair(uint32_t *a, uint32_t len)
{
	uint32_t    *b = a + len;
	uint32_t     tmp, need_sorting = 0;
	for (uint32_t i = 0; i < len; i++)
		if (need_sorting || a[i] > b[i])
		{
			need_sorting = 1;
			tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		}
		else if (a[i] < b[i])
			return;
}

__host__ void setheader(blake2b_state *ctx, const char *header, const u32 headerLen)
{
	uint32_t le_N = WN;
	uint32_t le_K = WK;
	uchar personal[] = "ZcashPoW01230123";
	memcpy(personal + 8, &le_N, 4);
	memcpy(personal + 12, &le_K, 4);
	blake2b_param P[1];
	P->digest_length = Equihash::HashOutput;
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
}
/*
			backet	slot	bits	word0	word1	word2	word3	word4	size	total		0	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15
basemap						32												4		0.25G	
stage_init																					M			I	I	I	I	I	I	I											round[0]
stage1		16		9		134		9		31		32		32		30		20		1.25G	M	P	P	I	I	I	I	I	I	I								pairs[0]	rount[0]	round[1]
stage2		16		9		109		15		32		32		30				16		1G		M	P	P	P	P						X	X	X	X				pairs[1]	round[1]	round[2]
stage3		16		9		84		22		32		30						12		0.75G	M	P	P	P	P	P	P	X	X	X								pairs[2]	round[2]	round[3]
stage4		16		9		59		29		30								8		0.5G	M	P	P	P	P	P	P	P	P		X	X						pairs[3]	round[3]	round[4]
stage_last	16		9		34		9		25								8		0.5G	M	P	P	P	P	P	P	P	P				X	X							round[4]

pairs					64												8		0.5G
*/
__host__ CudaSolver::CudaSolver(const std::string &aId, int aDeviceId)
	: _id(aId)
	, _deviceId(aDeviceId)
{
	const size_t memorySize = Equihash::MemoryUnitsCount * Equihash::MemoryUnitSize32 * sizeof(u32);
#if DEEP_CUDA_DEBUG
	_hostMemory.pairs[0] = nullptr;
	_hostMemory.round[0] = nullptr;
	_hostMemory.round[1] = nullptr;
	_hostMemory.round[2] = nullptr;
	_hostMemory.round[3] = nullptr;
	_hostMemory.round[4] = nullptr;
#endif
	_memory.units[0] = nullptr;

	ThrowIfCudaErrors(cudaGetDeviceProperties(&_deviceProps, _deviceId));

	if (_deviceProps.totalGlobalMem < (memorySize + sizeof(Items))) {
		LOG(Error) << "CUDA device " << std::string(_deviceProps.name) << " has insufficient GPU memory." << _deviceProps.totalGlobalMem << " bytes of memory found < " << (memorySize + sizeof(Items)) << " bytes of memory required";
		throw std::runtime_error("CUDA: failed to init");
	}

	ThrowIfCudaErrors(cudaSetDevice(_deviceId));
	ThrowIfCudaErrors(cudaDeviceReset());
	ThrowIfCudaErrors(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync/* | cudaDeviceMapHost*/));
	ThrowIfCudaErrors(cudaDeviceSetCacheConfig(cudaFuncCachePreferShared));
#if DEEP_CUDA_DEBUG
	if (cudaHostAlloc(&_hostEq, sizeof(equi), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaHostAlloc(&_hostMemory.baseMap, Equihash::BucketsCount*Equihash::SlotsCount * sizeof(u32), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaHostAlloc(&_hostMemory.pairs[0], 4 * Equihash::MemoryUnitSize32 * sizeof(uint2), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	_hostMemory.pairs[1] = _hostMemory.pairs[0] + Equihash::MemoryUnitSize32;
	_hostMemory.pairs[2] = _hostMemory.pairs[1] + Equihash::MemoryUnitSize32;
	_hostMemory.pairs[3] = _hostMemory.pairs[2] + Equihash::MemoryUnitSize32;

	if (cudaHostAlloc(&_hostMemory.round[0], Equihash::BucketsCount*Equihash::SlotsCount * sizeof(InitSlot), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaHostAlloc(&_hostMemory.round[1], Equihash::BucketsCount*Equihash::SlotsCount * sizeof(uint4), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaHostAlloc(&_hostMemory.round[2], Equihash::BucketsCount*Equihash::SlotsCount * sizeof(Slot3), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaHostAlloc(&_hostMemory.round[3], Equihash::BucketsCount*Equihash::SlotsCount * sizeof(uint2), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaHostAlloc(&_hostMemory.round[4], Equihash::BucketsCount*Equihash::SlotsCount * sizeof(uint2), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}
#else
	if (cudaHostAlloc(&_solutions, sizeof(scontainerreal), cudaHostAllocDefault) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}
#endif
	if (cudaMalloc((void**)&_deviceEq, sizeof(equi)) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.baseMap, Equihash::BucketsCount*Equihash::SlotsCount*sizeof(u32)) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	if (cudaMalloc(&_memory.units[0], memorySize) != cudaSuccess) {
		throw std::runtime_error("CUDA: failed to alloc memory");
	}

	_memory.units[1]  = _memory.units[0]  + Equihash::MemoryUnitSize32;
	_memory.units[2]  = _memory.units[1]  + Equihash::MemoryUnitSize32;
	_memory.units[3]  = _memory.units[2]  + Equihash::MemoryUnitSize32;
	_memory.units[4]  = _memory.units[3]  + Equihash::MemoryUnitSize32;
	_memory.units[5]  = _memory.units[4]  + Equihash::MemoryUnitSize32;
	_memory.units[6]  = _memory.units[5]  + Equihash::MemoryUnitSize32;
	_memory.units[7]  = _memory.units[6]  + Equihash::MemoryUnitSize32;
	_memory.units[8]  = _memory.units[7]  + Equihash::MemoryUnitSize32;
	_memory.units[9]  = _memory.units[8]  + Equihash::MemoryUnitSize32;
	_memory.units[10] = _memory.units[9]  + Equihash::MemoryUnitSize32;
	_memory.units[11] = _memory.units[10] + Equihash::MemoryUnitSize32;
	_memory.units[12] = _memory.units[11] + Equihash::MemoryUnitSize32;
}

#define	DO_METRICS	0

__host__ void CudaSolver::Solve(EquihashWork::Ref aWork, Listener &aListener)
{
	blake2b_state blake_ctx;

//	int blocks = Equihash::BucketsCount;
#if DO_METRICS
	LOG(Info) << "Equihash:";
	LOG(Info) << "\titems count        = " << Equihash::ItemsCount;
	LOG(Info) << "\tbucket bits        = " << Equihash::BucketBits;
	LOG(Info) << "\tbuckets count      = " << Equihash::BucketsCount;
	LOG(Info) << "\trest bits          = " << Equihash::RestBits;
	LOG(Info) << "\trests count        = " << Equihash::RestsCount;
	LOG(Info) << "\tslots count        = " << Equihash::SlotsCount;
//	LOG(Info) << "\tinit slots count   = " << Equihash::InitSlotsCount;
	LOG(Info) << "\tmemory unit        = " << (Equihash::MemoryUnitSize32/(1024*1024)) << "M u32 items";
	LOG(Info) << "\tunit size          = " << ((sizeof(u32)*Equihash::MemoryUnitSize32) / (1024 * 1024)) << "M";

	std::chrono::time_point<std::chrono::system_clock>	_start = std::chrono::system_clock::now();
	uint64_t cycleStart = __rdtsc();
#endif
	setheader(&blake_ctx, (const char *)aWork->GetData(), aWork->GetDataSize());
#if DO_METRICS
	uint64_t cycleSetHeader = __rdtsc();
#endif
#if BEAM_WORK_MODE
	ThrowIfCudaErrors(cudaMemcpy(&_deviceEq->blake_ctx, &blake_ctx, sizeof(blake_ctx), cudaMemcpyHostToDevice));
#else
	ThrowIfCudaErrors(cudaMemcpy(&_deviceEq->blake_h, &blake_ctx.h, sizeof(u64) * 8, cudaMemcpyHostToDevice));
#endif
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleCopyBlake = __rdtsc();
#endif
	ThrowIfCudaErrors(cudaMemset(&_deviceEq->edata, 0, sizeof(_deviceEq->edata)));
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleMemset = __rdtsc();
#endif
	stage_init << <(Equihash::BlakesCount + 511) / 512, 512 >> >(_deviceEq, (BaseMap)_memory.baseMap, (Items)_memory.units[2]);
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleDigit_0 = __rdtsc();
#endif
//	stage_1<RB, SM, SSM> << <Equihash::BucketsCount, 512 >> >(_deviceEq, _heap + 3 * Equihash::MemoryUnitSize32, _heap + 10 * Equihash::MemoryUnitSize32, (uint2*)(_heap + 1 * Equihash::MemoryUnitSize32));
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleDigit_1 = __rdtsc();
#endif
//	stage_2<RB, SM, SSM> << <Equihash::BucketsCount, 512 >> >(_deviceEq, (const uint4 *)(_heap + 10 * Equihash::MemoryUnitSize32), _heap + 7 * Equihash::MemoryUnitSize32, (uint2*)(_heap + 3 * Equihash::MemoryUnitSize32));
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleDigit_2 = __rdtsc();
#endif
//	stage_3<RB, SM, SSM> << <Equihash::BucketsCount, 512 >> >(_deviceEq, _heap + 7 * Equihash::MemoryUnitSize32, (uint2*)(_heap + 10 * Equihash::MemoryUnitSize32), (uint2*)(_heap + 5 * Equihash::MemoryUnitSize32));
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleDigit_3 = __rdtsc();
#endif
//	stage_4<RB, SM, SSM> << <Equihash::BucketsCount, 512 >> >(_deviceEq, (uint2*)(_heap + 10 * Equihash::MemoryUnitSize32), (uint2*)(_heap + 12 * Equihash::MemoryUnitSize32), (uint2*)(_heap + 7 * Equihash::MemoryUnitSize32));
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleDigit_4 = __rdtsc();
#endif
//	stage_last<RB, SM, SSM - 3> << <Equihash::BucketsCount, 512 >> >(_deviceEq, _heap, (uint2*)(_heap + 1 * Equihash::MemoryUnitSize32), (uint2*)(_heap + 12 * Equihash::MemoryUnitSize32));
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleDigit_5 = __rdtsc();
#endif
#if DEEP_CUDA_DEBUG
	ThrowIfCudaErrors(cudaMemcpy(_hostEq, _deviceEq, sizeof(equi), cudaMemcpyDeviceToHost));
	u32 m = 0;
	for (u32 i = 0; i < Equihash::BucketsCount; i++) {
		if (_hostEq->edata.nslots[0][i] > m) {
			m = _hostEq->edata.nslots[0][i];
		}
	}
	scontainerreal *_solutions = &_hostEq->edata.srealcont;
#else
	ThrowIfCudaErrors(cudaMemcpy(_solutions, &_deviceEq->edata.srealcont, sizeof(scontainerreal), cudaMemcpyDefault/*DeviceToHost*/));
#endif
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleCopySol = __rdtsc();

	double cycleTotal = cycleCopySol - cycleStart;

	LOG(Info) << "cycleSetHeader = " << (cycleSetHeader - cycleStart)     << " (" << ((cycleSetHeader - cycleStart) / cycleTotal) << ")";
	LOG(Info) << "cycleCopyBlake = " << (cycleCopyBlake - cycleSetHeader) << " (" << ((cycleCopyBlake - cycleSetHeader) / cycleTotal) << ")";
	LOG(Info) << "cycleMemset = "    << (cycleMemset - cycleCopyBlake)    << " (" << ((cycleMemset - cycleCopyBlake) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_0 = "   << (cycleDigit_0 - cycleMemset)      << " (" << ((cycleDigit_0 - cycleMemset) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_1 = "   << (cycleDigit_1 - cycleDigit_0)     << " (" << ((cycleDigit_1 - cycleDigit_0) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_2 = "   << (cycleDigit_2 - cycleDigit_1)     << " (" << ((cycleDigit_2 - cycleDigit_1) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_3 = "   << (cycleDigit_3 - cycleDigit_2)     << " (" << ((cycleDigit_3 - cycleDigit_2) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_4 = "   << (cycleDigit_4 - cycleDigit_3)     << " (" << ((cycleDigit_4 - cycleDigit_3) / cycleTotal) << ")";
	LOG(Info) << "cycleDigit_5 = "   << (cycleDigit_5 - cycleDigit_4)     << " (" << ((cycleDigit_5 - cycleDigit_4) / cycleTotal) << ")";
	LOG(Info) << "cycleCopySol = "   << (cycleCopySol - cycleDigit_5)     << " (" << ((cycleCopySol - cycleDigit_5) / cycleTotal) << ")";
	LOG(Info) << "cycleTotal = " << (uint64_t)cycleTotal;
	LOG(Info) << "time = " << (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count() << " ms";

	LOG(Info) << "Solutions count: " << _solutions->nsols;
#endif
	if (_solutions->nsols > MAXREALSOLS) {
		LOG(Info) << "Missing sols: " << (_solutions->nsols - MAXREALSOLS);
	}

	for (u32 s = 0; (s < _solutions->nsols) && (s < MAXREALSOLS); s++)
	{
		// remove dups on CPU (dup removal on GPU is not fully exact and can pass on some invalid _solutions)
		if (duped(_solutions->sols[s])) {
			continue;
		}

		// perform sort of pairs
		for (uint32_t level = 0; level < WK; level++) {
			for (uint32_t i = 0; i < (1 << WK); i += (2 << level)) {
				sort_pair(&_solutions->sols[s][i], 1 << level);
			}
		}

		std::vector<uint32_t> index_vector(PROOFSIZE);
		for (u32 i = 0; i < PROOFSIZE; i++) {
			index_vector[i] = _solutions->sols[s][i];
		}
#if DO_METRICS
		LOG(Info) << "Test solution #" << s << "/" << _solutions->nsols;
#endif
		aListener.OnSolution(*aWork, index_vector, Equihash::DigitBits);
	}

	aListener.OnHashDone();
}

__host__ void setBeamHader(blake2b_state &aState, const unsigned char *aData, unsigned aDataSize, const unsigned char *aNonce, unsigned aNonceSize)
{
	uint32_t le_N = 150;
	uint32_t le_K = 5;

	unsigned char personalization[BLAKE2B_PERSONALBYTES] = {};
	memcpy(personalization, "Beam-PoW", 8);
	memcpy(personalization + 8, &le_N, 4);
	memcpy(personalization + 12, &le_K, 4);

	const uint8_t outlen = (512 / 150) * ((150 + 7) / 8);

	static_assert(!((!outlen) || (outlen > BLAKE2B_OUTBYTES)), "!((!outlen) || (outlen > BLAKE2B_OUTBYTES))");

	blake2b_param param = { 0 };
	param.digest_length = outlen;
	param.fanout = 1;
	param.depth = 1;

	memcpy(&param.personal, personalization, BLAKE2B_PERSONALBYTES);

	blake2b_init_param(&aState, &param);
	blake2b_update(&aState, aData, aDataSize);
	blake2b_update(&aState, aNonce, aNonceSize);
}

__host__ void CudaSolver::Solve(BeamWork::Ref aWork, Listener &aListener)
{
	blake2b_state state;
#if DO_METRICS
	LOG(Info) << "Equihash:";
	LOG(Info) << "\titems count        = " << Equihash::ItemsCount;
	LOG(Info) << "\tbucket bits        = " << Equihash::BucketBits;
	LOG(Info) << "\tbuckets count      = " << Equihash::BucketsCount;
	LOG(Info) << "\trest bits          = " << Equihash::RestBits;
	LOG(Info) << "\trests count        = " << Equihash::RestsCount;
	LOG(Info) << "\tslots count        = " << Equihash::SlotsCount;
	LOG(Info) << "\tmemory unit        = " << (Equihash::MemoryUnitSize32 / (1024 * 1024)) << "M u32 items";
	LOG(Info) << "\tunit size          = " << ((sizeof(u32)*Equihash::MemoryUnitSize32) / (1024 * 1024)) << "M";

	std::chrono::time_point<std::chrono::system_clock>	_start = std::chrono::system_clock::now();
	uint64_t cycleStart = __rdtsc();
#endif
	setBeamHader(state, aWork->GetData(), aWork->GetDataSize(), aWork->GetNonce(), aWork->GetNonceSize());
#if DO_METRICS
	uint64_t cycleSetHeader = __rdtsc();
#endif
#if BEAM_WORK_MODE
	ThrowIfCudaErrors(cudaMemcpy(&_deviceEq->blake_ctx, &state, sizeof(blake2b_state), cudaMemcpyHostToDevice));
#else
	ThrowIfCudaErrors(cudaMemcpy(&_deviceEq->blake_h, &aState.h, sizeof(u64) * 8, cudaMemcpyHostToDevice));
#endif
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleCopyBlake = __rdtsc();
#endif
	ThrowIfCudaErrors(cudaMemset(&_deviceEq->edata, 0, sizeof(_deviceEq->edata)));
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleMemset = __rdtsc();
#endif
	stage_init << <(Equihash::BlakesCount + 255) / 256, 256 >> >(_deviceEq, (BaseMap)(_memory.baseMap), (Items)_memory.units[2]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_0 = __rdtsc();
#endif
	stage_1<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items)_memory.units[2], (Items4)_memory.units[9], (Pairs)_memory.units[0]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_1 = __rdtsc();
#endif
	stage_2<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items4)_memory.units[9], (Items3)_memory.units[6], (Pairs)_memory.units[2]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_2 = __rdtsc();
#endif
	stage_3<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items3)_memory.units[6], (Items2)_memory.units[9], (Pairs)_memory.units[4]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_3 = __rdtsc();
#endif
	stage_4<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items2)_memory.units[9], (Items2)_memory.units[11], (Pairs)_memory.units[6]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_4 = __rdtsc();
#endif
	stage_last<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (BaseMap)(_memory.baseMap), (uint2*)(_memory.units[0]), (Items2)_memory.units[11]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_5 = __rdtsc();
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
	ThrowIfCudaErrors(cudaMemcpy(_solutions, &_deviceEq->edata.srealcont, sizeof(scontainerreal), cudaMemcpyDeviceToHost));
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
	LOG(Info) << "cycleCopySol = " << (cycleCopySol - cycleDigit_5) << " (" << ((cycleCopySol - cycleDigit_5) / cycleTotal) << ")";
	LOG(Info) << "cycleTotal = " << (uint64_t)cycleTotal;
	LOG(Info) << "time = " << (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count() << " ms";

	LOG(Info) << "Solutions count: " << _solutions->nsols;
#endif
	if (_solutions->nsols > MAXREALSOLS) {
		LOG(Info) << "Missing sols: " << (_solutions->nsols - MAXREALSOLS);
	}

	for (u32 s = 0; (s < _solutions->nsols) && (s < MAXREALSOLS); s++)
	{
		// remove dups on CPU (dup removal on GPU is not fully exact and can pass on some invalid _solutions)
		if (duped(_solutions->sols[s])) {
			continue;
		}
#if 1
		// perform sort of pairs
		for (uint32_t level = 0; level < WK; level++) {
			for (uint32_t i = 0; i < (1 << WK); i += (2 << level)) {
				sort_pair(&_solutions->sols[s][i], 1 << level);
			}
		}
#endif
		std::vector<uint32_t> index_vector(PROOFSIZE);
		for (u32 i = 0; i < PROOFSIZE; i++) {
			index_vector[i] = _solutions->sols[s][i];
		}
#if DO_METRICS
		LOG(Info) << "Test solution #" << s << "/" << _solutions->nsols;
#endif
		aListener.OnSolution(*aWork, index_vector, Equihash::DigitBits);
	}

	aListener.OnHashDone();
}
#if DEEP_CUDA_DEBUG
//=================================================================================================

#include "CudaSolver-150-5-debug.cuh"

//=================================================================================================
#else
__host__ void CudaSolver::Test(blake2b_state &aState, Listener &aListener)
{
#if DO_METRICS
	LOG(Info) << "Equihash:";
	LOG(Info) << "\titems count        = " << Equihash::ItemsCount;
	LOG(Info) << "\tbucket bits        = " << Equihash::BucketBits;
	LOG(Info) << "\tbuckets count      = " << Equihash::BucketsCount;
	LOG(Info) << "\trest bits          = " << Equihash::RestBits;
	LOG(Info) << "\trests count        = " << Equihash::RestsCount;
	LOG(Info) << "\tslots count        = " << Equihash::SlotsCount;
	LOG(Info) << "\tmemory unit        = " << (Equihash::MemoryUnitSize32 / (1024 * 1024)) << "M u32 items";
	LOG(Info) << "\tunit size          = " << ((sizeof(u32)*Equihash::MemoryUnitSize32) / (1024 * 1024)) << "M";

	std::chrono::time_point<std::chrono::system_clock>	_start = std::chrono::system_clock::now();
	uint64_t cycleStart = __rdtsc();
#endif
#if DO_METRICS
	uint64_t cycleSetHeader = __rdtsc();
#endif
#if BEAM_WORK_MODE
	ThrowIfCudaErrors(cudaMemcpy(&_deviceEq->blake_ctx, &aState, sizeof(blake2b_state), cudaMemcpyHostToDevice));
#else
	ThrowIfCudaErrors(cudaMemcpy(&_deviceEq->blake_h, &aState.h, sizeof(u64) * 8, cudaMemcpyHostToDevice));
#endif
#if DO_METRICS
	cudaDeviceSynchronize();
	uint64_t cycleCopyBlake = __rdtsc();
#endif
	ThrowIfCudaErrors(cudaMemset(&_deviceEq->edata, 0, sizeof(_deviceEq->edata)));
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleMemset = __rdtsc();
#endif
	stage_init << <(Equihash::BlakesCount + 255) / 256, 256 >> >(_deviceEq, (BaseMap)(_memory.baseMap), (Items)_memory.units[2]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_0 = __rdtsc();
#endif
	stage_1<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items)_memory.units[2], (Items4)_memory.units[9], (Pairs)_memory.units[0]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_1 = __rdtsc();
#endif
	stage_2<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items4)_memory.units[9], (Items3)_memory.units[6], (Pairs)_memory.units[2]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_2 = __rdtsc();
#endif
	stage_3<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items3)_memory.units[6], (Items2)_memory.units[9], (Pairs)_memory.units[4]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_3 = __rdtsc();
#endif
	stage_4<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (Items2)_memory.units[9], (Items2)_memory.units[11], (Pairs)_memory.units[6]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_4 = __rdtsc();
#endif
	stage_last<SSM> << < Equihash::BucketsCount, 256 >> >(_deviceEq, (BaseMap)(_memory.baseMap), (uint2*)(_memory.units[0]), (Items2)_memory.units[11]);
#if DO_METRICS
	ThrowIfCudaErrors(cudaDeviceSynchronize());
	uint64_t cycleDigit_5 = __rdtsc();
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
	ThrowIfCudaErrors(cudaMemcpy(_solutions, &_deviceEq->edata.srealcont, sizeof(scontainerreal), cudaMemcpyDeviceToHost));
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
	LOG(Info) << "cycleCopySol = " << (cycleCopySol - cycleDigit_5) << " (" << ((cycleCopySol - cycleDigit_5) / cycleTotal) << ")";
	LOG(Info) << "cycleTotal = " << (uint64_t)cycleTotal;
	LOG(Info) << "time = " << (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count() << " ms";

	LOG(Info) << "Solutions count: " << _solutions->nsols;
#endif
	if (_solutions->nsols > MAXREALSOLS) {
		LOG(Info) << "Missing sols: " << (_solutions->nsols - MAXREALSOLS);
	}

	for (u32 s = 0; (s < _solutions->nsols) && (s < MAXREALSOLS); s++)
	{
		// remove dups on CPU (dup removal on GPU is not fully exact and can pass on some invalid _solutions)
		if (duped(_solutions->sols[s])) {
			continue;
		}
#if 1
		// perform sort of pairs
		for (uint32_t level = 0; level < WK; level++) {
			for (uint32_t i = 0; i < (1 << WK); i += (2 << level)) {
				sort_pair(&_solutions->sols[s][i], 1 << level);
			}
		}
#endif
		std::vector<uint32_t> index_vector(PROOFSIZE);
		for (u32 i = 0; i < PROOFSIZE; i++) {
			index_vector[i] = _solutions->sols[s][i];
		}
#if DO_METRICS
		LOG(Info) << "Test solution #" << s << "/" << _solutions->nsols;
#endif
		aListener.OnSolution(EquihashWork(), index_vector, Equihash::DigitBits);
	}

	aListener.OnHashDone();
}
#endif
__host__ CudaSolver::~CudaSolver()
{
#if DEEP_CUDA_DEBUG
	if (_hostEq) {
		cudaFreeHost(_hostEq);
		_hostEq = nullptr;
	}

	if (_hostMemory.baseMap) {
		cudaFreeHost(_hostMemory.baseMap);
		_hostMemory.baseMap = nullptr;
	}

	if (_hostMemory.pairs[0]) {
		cudaFreeHost(_hostMemory.pairs[0]);
		_hostMemory.pairs[0] = nullptr;
	}

	if (_hostMemory.round[0]) {
		cudaFreeHost(_hostMemory.round[0]);
		_hostMemory.round[0] = nullptr;
	}

	if (_hostMemory.round[1]) {
		cudaFreeHost(_hostMemory.round[1]);
		_hostMemory.round[1] = nullptr;
	}

	if (_hostMemory.round[2]) {
		cudaFreeHost(_hostMemory.round[2]);
		_hostMemory.round[2] = nullptr;
	}

	if (_hostMemory.round[3]) {
		cudaFreeHost(_hostMemory.round[3]);
		_hostMemory.round[3] = nullptr;
	}

	if (_hostMemory.round[4]) {
		cudaFreeHost(_hostMemory.round[4]);
		_hostMemory.round[4] = nullptr;
	}
#else
	if (_solutions) {
		cudaFree(_solutions);
		_solutions = nullptr;
	}
#endif
	if (_memory.baseMap) {
		cudaFree(_memory.baseMap);
		_memory.baseMap = nullptr;
	}
	
	if (_memory.units[0]) {
		cudaFree(_memory.units[0]);
		_memory.units[0] = nullptr;
	}

	cudaFree(_deviceEq);
	cudaDeviceReset();
}

#ifdef CONFIG_MODE_1
template class CudaSolver<CONFIG_MODE_1>;
#endif

