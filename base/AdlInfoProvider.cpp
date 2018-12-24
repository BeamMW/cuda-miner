/*
* Wrapper for ADL, inspired by wrapnvml from John E. Stone
*
* By Philipp Andreas - github@smurfy.de
*/
#include "pch.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wraphelper.h"
#include "AdlInfoProvider.hpp"
#include "Logging.hpp"

#if USE_OPENCL

#include "base/cl2.hpp"

void* ADL_API_CALL ADL_Main_Memory_Alloc(int iSize)
{
	void* lpBuffer = malloc(iSize);
	return lpBuffer;
}

AdlInfoProvider::Ref AdlInfoProvider::create()
{
	AdlInfoProvider::Ref provider;
#ifdef _WIN32
#define libatiadlxx "atiadlxx.dll"
#else
#define libatiadlxx "libatiadlxx.so"
#endif
	void *adl_dll = wrap_dlopen(libatiadlxx);
	if (adl_dll == NULL) {
		return Ref();
	}

	provider = new AdlInfoProvider();

	provider->adl_dll = adl_dll;

	provider->adlMainControlCreate = (wrap_adlReturn_t(*)(ADL_MAIN_MALLOC_CALLBACK, int))wrap_dlsym(provider->adl_dll, "ADL_Main_Control_Create");
	provider->adlAdapterNumberOfAdapters = (wrap_adlReturn_t(*)(int *))wrap_dlsym(provider->adl_dll, "ADL_Adapter_NumberOfAdapters_Get");
	provider->adlAdapterAdapterInfoGet = (wrap_adlReturn_t(*)(ADLAdapterInfo*, int))wrap_dlsym(provider->adl_dll, "ADL_Adapter_AdapterInfo_Get");
	provider->adlAdapterAdapterIdGet = (wrap_adlReturn_t(*)(int, int*))wrap_dlsym(provider->adl_dll, "ADL_Adapter_ID_Get");
	provider->adlOverdrive5TemperatureGet = (wrap_adlReturn_t(*)(int, int, ADLTemperature*))wrap_dlsym(provider->adl_dll, "ADL_Overdrive5_Temperature_Get");
	provider->adlOverdrive5FanSpeedGet = (wrap_adlReturn_t(*)(int, int, ADLFanSpeedValue*))wrap_dlsym(provider->adl_dll, "ADL_Overdrive5_FanSpeed_Get");
	provider->adlMainControlRefresh = (wrap_adlReturn_t(*)(void))wrap_dlsym(provider->adl_dll, "ADL_Main_Control_Refresh");
	provider->adlMainControlDestory = (wrap_adlReturn_t(*)(void))wrap_dlsym(provider->adl_dll, "ADL_Main_Control_Destroy");

	if (provider->adlMainControlCreate == NULL ||
		provider->adlMainControlDestory == NULL ||
		provider->adlMainControlRefresh == NULL ||
		provider->adlAdapterNumberOfAdapters == NULL ||
		provider->adlAdapterAdapterInfoGet == NULL ||
		provider->adlAdapterAdapterIdGet == NULL ||
		provider->adlOverdrive5TemperatureGet == NULL ||
		provider->adlOverdrive5FanSpeedGet == NULL
		) {
#if 1
		LOG(Error) << "Failed to obtain all required ADL function pointers";
#endif
		return Ref();
	}

	provider->adlMainControlCreate(ADL_Main_Memory_Alloc, 1);
	provider->adlMainControlRefresh();

	// Get and count OpenCL devices.
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	std::vector<cl::Device> platdevs;
	for (unsigned p = 0; p < platforms.size(); p++)
	{
		std::string platformName = platforms[p].getInfo<CL_PLATFORM_NAME>();
		if (platformName == "AMD Accelerated Parallel Processing")
		{
			platforms[p].getDevices(CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR, &platdevs);
			break;
		}
	}

	if (0 == platdevs.size()) {
		return Ref();
	}

	for (unsigned j = 0; j < platdevs.size(); j++)
	{
		cl::Device cldev = platdevs[j];
		cl_device_topology_amd topology;
		int status = clGetDeviceInfo(cldev(), CL_DEVICE_TOPOLOGY_AMD, sizeof(cl_device_topology_amd), &topology, nullptr);
		if (status == CL_SUCCESS)
		{
			if (topology.raw.type == CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD)
			{
				provider->_gpus.emplace_back();
				provider->_gpus.back().iAdapterIndex = -1;
				provider->_gpus.back().iBusNumber = topology.pcie.bus;
				provider->_gpus.back().iDeviceNumber = topology.pcie.device;
				provider->_gpus.back().iFunctionNumber = topology.pcie.function;
				provider->_gpus.back().iVendorID = 0x1002;
				if (CL_SUCCESS != clGetDeviceInfo(cldev(), CL_DEVICE_BOARD_NAME_AMD, sizeof(provider->_gpus.back().strAdapterName), &provider->_gpus.back().strAdapterName, NULL)) {
					provider->_gpus.back().strAdapterName[0] = 0;
				}
#if 0
					printf("[DEBUG] - SYSFS GPU[%d]%d,%d,%d matches OpenCL GPU[%d]%d,%d,%d\n",
						i,
						iBus,
						iDevice,
						iFunction,
						j, (int)topology.pcie.bus, (int)topology.pcie.device, (int)topology.pcie.function);
#endif
			}
		}
	}

	int logicalGpuCount = 0;
	provider->adlAdapterNumberOfAdapters(&logicalGpuCount);
	int last_adapter = 0;

	if (logicalGpuCount > 0) {
		std::vector<ADLAdapterInfo> devs(logicalGpuCount);
		memset(&devs.front(), '\0', sizeof(ADLAdapterInfo) * logicalGpuCount);

		devs[0].iSize = sizeof(ADLAdapterInfo);

		int res = provider->adlAdapterAdapterInfoGet(&devs.front(), sizeof(ADLAdapterInfo) * logicalGpuCount);

		for (int i = 0; i < logicalGpuCount; i++) {
			int adapterIndex = devs[i].iAdapterIndex;
			int adapterID = 0;

			res = provider->adlAdapterAdapterIdGet(adapterIndex, &adapterID);

			if (res != WRAPADL_OK) {
				continue;
			}

			if (adapterID == last_adapter) {
				continue;
			}

			last_adapter = adapterID;
			for (auto &clDev : provider->_gpus) {
				if ((devs[i].iBusNumber == clDev.iBusNumber) && (devs[i].iDeviceNumber == clDev.iDeviceNumber) && (devs[i].iFunctionNumber == clDev.iFunctionNumber)) {
					clDev.iAdapterIndex = adapterIndex;
				}
			}
		}
	}

	return provider;
}

AdlInfoProvider::~AdlInfoProvider()
{
	if (adl_dll) {
		adlMainControlDestory();
		wrap_dlclose(adl_dll);
		adl_dll = nullptr;
	}
}

bool AdlInfoProvider::getGpuName(int aGpuIndex, char *namebuf, int bufsize)
{
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size()) {
		return false;
	}

	memcpy(namebuf, _gpus[aGpuIndex].strAdapterName, bufsize);
	return true;
}

bool AdlInfoProvider::getTempC(int aGpuIndex, unsigned int *tempC)
{
	wrap_adlReturn_t rc;
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || -1 == _gpus[aGpuIndex].iAdapterIndex) {
		return false;
	}

	ADLTemperature temperature;
	temperature.iSize = sizeof(ADLTemperature);
	rc = adlOverdrive5TemperatureGet(_gpus[aGpuIndex].iAdapterIndex, 0, &temperature);
	if (rc != WRAPADL_OK) {
		return false;
	}
	*tempC = unsigned(temperature.iTemperature / 1000);
	return true;
}

bool AdlInfoProvider::getFanPcnt(int aGpuIndex, unsigned int *fanpcnt)
{
	wrap_adlReturn_t rc;
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || -1 == _gpus[aGpuIndex].iAdapterIndex) {
		return false;
	}

	ADLFanSpeedValue fan;
	fan.iSize = sizeof(ADLFanSpeedValue);
	fan.iSpeedType = 1;
	rc = adlOverdrive5FanSpeedGet(_gpus[aGpuIndex].iAdapterIndex, 0, &fan);
	if (rc != WRAPADL_OK) {
		return false;
	}
	*fanpcnt = unsigned(fan.iFanSpeed);
	return true;
}

bool AdlInfoProvider::getPowerUsage(int aGpuIndex, unsigned int *milliwatts)
{
	*milliwatts = 0;
	return true;
}
#endif
