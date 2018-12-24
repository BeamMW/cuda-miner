/*
 * Wrapper for AMD SysFS on linux, using adapted code from amdcovc by matszpk
 *
 * By Philipp Andreas - github@smurfy.de
 */

#pragma once

#pragma	once

#if USE_OPENCL
#include "InfoProvider.hpp"

typedef struct
{
    int sysfs_gpucount;
    int opencl_gpucount;
    int* card_sysfs_device_id;   /* map cardidx to filesystem card idx */
    int* sysfs_hwmon_id;         /* filesystem card idx to filesystem hwmon idx */
    int* sysfs_opencl_device_id; /* map ADL dev to OPENCL dev */
    int* opencl_sysfs_device_id; /* map OPENCL dev to ADL dev */
} wrap_amdsysfs_handle;

class AmdSysFsInfoProvider : public InfoProvider
{
public:
	typedef Reference<AmdSysFsInfoProvider> Ref;

public:
	AmdSysFsInfoProvider();

	bool getGpuName(int aGpuIndex, char *namebuf, int bufsize) override;

	bool getTempC(int aGpuIndex, unsigned int *tempC) override;

	bool getFanPcnt(int aGpuIndex, unsigned int *fanpcnt) override;

	bool getPowerUsage(int aGpuIndex, unsigned int *milliwatts) override;

	static Ref create();

protected:
	wrap_amdsysfs_handle		_sysfsh;
};

#endif
