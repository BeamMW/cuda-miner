#include "pch.hpp"
#include <stdio.h>
#include <stdlib.h>
#include "wraphelper.h"
#include "NvidiaInfoProvider.hpp"
#include "Logging.hpp"

#if USE_CUDA
#include <cuda_runtime.h>

NvidiaInfoProvider::Ref NvidiaInfoProvider::create()
{
	int i = 0;
	NvidiaInfoProvider::Ref provider;

	/*
	 * We use hard-coded library installation locations for the time being...
	 * No idea where or if libnvidia-ml.so is installed on MacOS X, a
	 * deep scouring of the filesystem on one of the Mac CUDA build boxes
	 * I used turned up nothing, so for now it's not going to work on OSX.
	 */
#if defined(_WIN64)
	 /* 64-bit Windows */
#define  libnvidia_ml "%PROGRAMFILES%/NVIDIA Corporation/NVSMI/nvml.dll"
#elif defined(_WIN32) || defined(_MSC_VER)
	 /* 32-bit Windows */
#define  libnvidia_ml "%PROGRAMFILES%/NVIDIA Corporation/NVSMI/nvml.dll"
#elif defined(__linux) && (defined(__i386__) || defined(__ARM_ARCH_7A__))
	 /* 32-bit linux assumed */
#define  libnvidia_ml "libnvidia-ml.so"
#elif defined(__linux)
	 /* 64-bit linux assumed */
#define  libnvidia_ml "libnvidia-ml.so"
#else
#define  libnvidia_ml ""
	#warning "Unrecognized platform: need NVML DLL path for this platform..."
	return NULL;
#endif

#ifdef _WIN32
	char tmp[512];
	ExpandEnvironmentStringsA(libnvidia_ml, tmp, sizeof(tmp));
#else
	char tmp[512] = libnvidia_ml;
#endif

	void *nvml_dll = wrap_dlopen(tmp);
	if (nvml_dll == NULL) {
		LOG(Error) << "Can't open " << tmp;
		return Ref();
	}

	provider = new NvidiaInfoProvider;

	provider->nvml_dll = nvml_dll;

	provider->nvmlInit = (wrap_nvmlReturn_t(*)(void))wrap_dlsym(provider->nvml_dll, "nvmlInit");
	provider->nvmlDeviceGetCount = (wrap_nvmlReturn_t(*)(int *))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetCount_v2");
	provider->nvmlDeviceGetHandleByIndex = (wrap_nvmlReturn_t(*)(int, wrap_nvmlDevice_t *))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetHandleByIndex_v2");
	provider->nvmlDeviceGetPciInfo = (wrap_nvmlReturn_t(*)(wrap_nvmlDevice_t, PciInfo *))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetPciInfo");
	provider->nvmlDeviceGetName = (wrap_nvmlReturn_t(*)(wrap_nvmlDevice_t, char *, int))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetName");
	provider->nvmlDeviceGetTemperature = (wrap_nvmlReturn_t(*)(wrap_nvmlDevice_t, int, unsigned int *))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetTemperature");
	provider->nvmlDeviceGetFanSpeed = (wrap_nvmlReturn_t(*)(wrap_nvmlDevice_t, unsigned int *))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetFanSpeed");
	provider->nvmlDeviceGetPowerUsage = (wrap_nvmlReturn_t(*)(wrap_nvmlDevice_t, unsigned int *))wrap_dlsym(provider->nvml_dll, "nvmlDeviceGetPowerUsage");
	provider->nvmlShutdown = (wrap_nvmlReturn_t(*)())wrap_dlsym(provider->nvml_dll, "nvmlShutdown");

	if (provider->nvmlInit == NULL ||
		provider->nvmlShutdown == NULL ||
		provider->nvmlDeviceGetCount == NULL ||
		provider->nvmlDeviceGetHandleByIndex == NULL ||
		provider->nvmlDeviceGetPciInfo == NULL ||
		provider->nvmlDeviceGetName == NULL ||
		provider->nvmlDeviceGetTemperature == NULL ||
		provider->nvmlDeviceGetFanSpeed == NULL ||
		provider->nvmlDeviceGetPowerUsage == NULL
		)
	{
#if 0
		printf("Failed to obtain all required NVML function pointers\n");
#endif
		wrap_dlclose(provider->nvml_dll);
		return Ref();
	}

	int nvml_gpucount = 0;
	provider->nvmlInit();
	provider->nvmlDeviceGetCount(&nvml_gpucount);

	int cuda_gpucount = 0;
	/* Query CUDA device count, in case it doesn't agree with NVML, since  */
	/* CUDA will only report GPUs with compute capability greater than 1.0 */
	cudaError_t err = cudaGetDeviceCount(&cuda_gpucount);
	if (cudaSuccess != err) {
		if (err == cudaErrorInsufficientDriver)
		{
			int driverVersion = 0;
			cudaDriverGetVersion(&driverVersion);
			if (driverVersion == 0) {
				LOG(Error) << "CUDA Error: No CUDA driver found";
			}
			else {
				LOG(Error) << "CUDA Error: Insufficient CUDA driver: " << driverVersion;
			}
		}
		LOG(Error) << "CUDA Error: " << cudaGetErrorString(err);
		return Ref();
	}

	provider->_gpus.resize(cuda_gpucount);
	for (int i = 0; i < cuda_gpucount; i++) {
		cudaDeviceProp props;
		if (cudaGetDeviceProperties(&props, i) == cudaSuccess) {
			provider->_gpus[i].iAdapterIndex = -1;
			provider->_gpus[i].extra.hDevice = nullptr;
			provider->_gpus[i].iBusNumber = props.pciBusID;
			provider->_gpus[i].iDeviceNumber = props.pciDeviceID;
			provider->_gpus[i].iFunctionNumber = props.pciDomainID;
			provider->_gpus[i].iVendorID = 0;
			strncpy(provider->_gpus[i].strAdapterName, props.name, sizeof(provider->_gpus[i].strAdapterName));
		}
	}

	/* Obtain GPU device handles we're going to need repeatedly... */
	for (int i = 0; i < nvml_gpucount; i++) {
		wrap_nvmlDevice_t nvmlDevice;
		provider->nvmlDeviceGetHandleByIndex(i, &nvmlDevice);
		PciInfo pciInfo;
		provider->nvmlDeviceGetPciInfo(nvmlDevice, &pciInfo);
		for (auto &clDev : provider->_gpus) {
			if ((pciInfo.domain == clDev.iFunctionNumber) &&
				(pciInfo.bus == clDev.iBusNumber) &&
				(pciInfo.device == clDev.iDeviceNumber)) {
#if 0
				printf("CUDA GPU[%d] matches NVML GPU[%d]\n", i, j);
#endif
				clDev.iAdapterIndex = i;
				clDev.extra.hDevice = nvmlDevice;
			}
		}
	}

	return provider;
}

NvidiaInfoProvider::~NvidiaInfoProvider()
{
	if (nullptr != nvml_dll) {
		if (nullptr != nvmlShutdown) {
			nvmlShutdown();
		}
		wrap_dlclose(nvml_dll);
		nvml_dll = nullptr;
	}
}

bool NvidiaInfoProvider::getGpuName(int aGpuIndex, char *namebuf, int bufsize)
{
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size()) {
		return false;
	}

	memcpy(namebuf, _gpus[aGpuIndex].strAdapterName, bufsize);
#if 0
	if (nvmlDeviceGetName(devs[gpuindex], namebuf, bufsize) != WRAPNVML_SUCCESS) {
		return false;
	}
#endif
	return true;
}

bool NvidiaInfoProvider::getTempC(int aGpuIndex, unsigned int *tempC)
{
	wrap_nvmlReturn_t rc;
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || nullptr == _gpus[aGpuIndex].extra.hDevice) {
		return false;
	}

	rc = nvmlDeviceGetTemperature(_gpus[aGpuIndex].extra.hDevice, 0u /* NVML_TEMPERATURE_GPU */, tempC);
	if (rc != WRAPNVML_SUCCESS) {
		return false;
	}

	return true;
}

bool NvidiaInfoProvider::getFanPcnt(int aGpuIndex, unsigned int *fanpcnt)
{
	wrap_nvmlReturn_t rc;
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || nullptr == _gpus[aGpuIndex].extra.hDevice) {
		return false;
	}

	rc = nvmlDeviceGetFanSpeed(_gpus[aGpuIndex].extra.hDevice, fanpcnt);
	if (rc != WRAPNVML_SUCCESS) {
		return false;
	}

	return true;
}

bool NvidiaInfoProvider::getPowerUsage(int aGpuIndex, unsigned int *milliwatts)
{
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || nullptr == _gpus[aGpuIndex].extra.hDevice) {
		return false;
	}

	if (nvmlDeviceGetPowerUsage(_gpus[aGpuIndex].extra.hDevice, milliwatts) != WRAPNVML_SUCCESS) {
		return false;
	}

	return true;
}
#endif
