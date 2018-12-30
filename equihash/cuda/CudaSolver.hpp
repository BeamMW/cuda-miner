#pragma once

#include "cuda.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "../blake2/blake2.h"
#include "../Solver.hpp"

#if !defined(_WIN32) && !defined(WIN32)
#define	_snprintf snprintf
#endif

#define ThrowIfCudaErrors(call)					\
do {											\
	cudaError_t err = call;						\
	if (cudaSuccess != err) {					\
		char errorBuff[512];					\
        _snprintf(errorBuff, sizeof(errorBuff) - 1, "CUDA error '%s' in func '%s' line %d", cudaGetErrorString(err), __FUNCTION__, __LINE__);	\
		throw std::runtime_error(errorBuff);	\
		}										\
} while (0)

#define ThrowIfCudaDriverErrors(call)			\
do {											\
	CUresult err = call;						\
	if (CUDA_SUCCESS != err) {					\
		char errorBuff[512];					\
		_snprintf(errorBuff, sizeof(errorBuff) - 1, "CUDA error DRIVER: '%d' in func '%s' line %d", err, __FUNCTION__, __LINE__);	\
		throw std::runtime_error(errorBuff);	\
	}											\
} while (0)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef unsigned char uchar;

struct packer_default;
struct packer_cantor;

#define	DEEP_CUDA_DEBUG	0

#define WN				150
#define WK				5
#define PROOFSIZE		(1<<WK)
#define MAXREALSOLS		128

struct scontainerreal
{
	u32 sols[MAXREALSOLS][PROOFSIZE];
	u32 nsols;
};
#if WN == 150 && WK == 5
struct equi;
struct slot;

struct CudaSolver : public Solver
{
	const static u32 SSM = 12;

	CudaSolver(const std::string &aId, int aDeviceId);
	~CudaSolver() override;

	void Solve(EquihashWork::Ref aWork, Listener &aListener) override;
	void Solve(BeamWork::Ref aWork, Listener &aListener) override;
	void Test(blake2b_state &aState, Listener &aListener);

	const cudaDeviceProp & GetDeviceProperties() const { return _deviceProps; }

protected:
	std::string			_id;
	int					_deviceId;
	cudaDeviceProp		_deviceProps;
	equi				*_deviceEq;
#if DEEP_CUDA_DEBUG
	equi				*_hostEq;
	struct {
		u32		*baseMap = nullptr;
		uint2	*pairs[4];
		u32		*round[5];
	}					_hostMemory;
#else
	scontainerreal	*_solutions = nullptr;
#endif
	struct {
		u32		*baseMap = nullptr;
		u32		*units[13];
	}					_memory;
};
#else
template <u32 RB, u32 SM>
struct equi;

struct slot;

template <u32 RB, u32 SM, u32 SSM, u32 THREADS, typename PACKER>
struct CudaSolver : public Solver
{
	CudaSolver(const std::string &aId, int aDeviceId);
	~CudaSolver() override;

	void Solve(EquihashWork::Ref aWork, Listener &aListener) override;
	void Test(blake2b_state &aState, Listener &aListener);

	const cudaDeviceProp & GetDeviceProperties() const { return _deviceProps; }

protected:
	std::string			_id;
	int					_deviceId;
	cudaDeviceProp		_deviceProps;
	equi<RB, SM>		*_deviceEq;
#if WN == 150 && WK == 5
	equi<RB, SM>		*_hostEq;
	struct {
		u32		*baseMap = nullptr;
		uint2	*pairs[4];
		u32		*round[5];
	}					_hostMemory;
	struct {
		u32		*baseMap = nullptr;
		u32		*units[13];
	}					_memory;
#else
	scontainerreal		*_solutions = nullptr;
#endif
};

#define CONFIG_MODE_1	9, 1248, 12, 640, packer_cantor
#if 0
#define CONFIG_MODE_2	8, 640, 12, 512, packer_default
#endif
#define CONFIG_MODE_3	9, 1248, 10, 640, packer_cantor
#endif
