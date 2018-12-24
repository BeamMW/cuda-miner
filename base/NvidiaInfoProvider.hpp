/*
 * A trivial little dlopen()-based wrapper library for the 
 * NVIDIA NVML library, to allow runtime discovery of NVML on an
 * arbitrary system.  This is all very hackish and simple-minded, but
 * it serves my immediate needs in the short term until NVIDIA provides 
 * a static NVML wrapper library themselves, hopefully in 
 * CUDA 6.5 or maybe sometime shortly after. 
 *
 * This trivial code is made available under the "new" 3-clause BSD license,
 * and/or any of the GPL licenses you prefer.
 * Feel free to use the code and modify as you see fit.
 *
 * John E. Stone - john.stone@gmail.com
 *
 */
#pragma	once

#if USE_CUDA
#include "InfoProvider.hpp"

/* 
 * Ugly hacks to avoid dependencies on the real nvml.h until it starts
 * getting included with the CUDA toolkit or a GDK that's got a known 
 * install location, etc.
 */
typedef enum wrap_nvmlReturn_enum {
  WRAPNVML_SUCCESS = 0
} wrap_nvmlReturn_t;

typedef void * wrap_nvmlDevice_t;

class NvidiaInfoProvider : public InfoProvider
{
public:
	/* our own version of the PCI info struct */
	struct PciInfo
	{
		char bus_id_str[16];             /* string form of bus info */
		unsigned int domain;
		unsigned int bus;
		unsigned int device;
		unsigned int pci_device_id;      /* combined device and vendor id */
		unsigned int pci_subsystem_id;
		unsigned int res0;               /* NVML internal use only */
		unsigned int res1;
		unsigned int res2;
		unsigned int res3;
	};

protected:
	void *nvml_dll = nullptr;
	wrap_nvmlReturn_t(*nvmlInit)(void) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetCount)(int *) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetHandleByIndex)(int, wrap_nvmlDevice_t *) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetPciInfo)(wrap_nvmlDevice_t, PciInfo *) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetName)(wrap_nvmlDevice_t, char *, int) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetTemperature)(wrap_nvmlDevice_t, int, unsigned int *) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetFanSpeed)(wrap_nvmlDevice_t, unsigned int *) = nullptr;
	wrap_nvmlReturn_t(*nvmlDeviceGetPowerUsage)(wrap_nvmlDevice_t, unsigned int *) = nullptr;
	wrap_nvmlReturn_t(*nvmlShutdown)(void) = nullptr;

public:
	typedef Reference<NvidiaInfoProvider> Ref;

public:
	~NvidiaInfoProvider() override;

	/*
	* query the name of the GPU model from the CUDA device ID
	*
	*/
	bool getGpuName(int aGpuIndex, char *namebuf, int bufsize);

	/*
	* Query the current GPU temperature (Celsius), from the CUDA device ID
	*/
	bool getTempC(int aGpuIndex, unsigned int *tempC);

	/*
	* Query the current GPU fan speed (percent) from the CUDA device ID
	*/
	bool getFanPcnt(int aGpuIndex, unsigned int *fanpcnt);

	/*
	* Query the current GPU power usage in millwatts from the CUDA device ID
	*
	* This feature is only available on recent GPU generations and may be
	* limited in some cases only to Tesla series GPUs.
	* If the query is run on an unsupported GPU, this routine will return -1.
	*/
	bool getPowerUsage(int aGpuIndex, unsigned int *milliwatts);

public:
	static NvidiaInfoProvider::Ref create();
};
#endif
