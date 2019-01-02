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

struct equi;
struct slot;

struct CudaSolver : public Solver
{
	typedef Reference<CudaSolver> Ref;

	const static u32 SSM = 12;

	CudaSolver(const std::string &aId, int aDeviceId);
	~CudaSolver() override;

	void Solve(BeamWork::Ref aWork, Listener &aListener) override;

	const cudaDeviceProp & GetDeviceProperties() const { return _deviceProps; }

protected:
	std::string			_id;
	int					_deviceId;
	cudaDeviceProp		_deviceProps;
	u32					*_solutions = nullptr;
	struct {
		uint4		*unit0 = nullptr;
		uint4		*unit1 = nullptr;
		uint4		*unit2 = nullptr;
		uint2		*unit3 = nullptr;
		uint4		*unit4 = nullptr;
		u32			*unit5 = nullptr;
		u32			*unit6 = nullptr;
	}					_memory;
};
