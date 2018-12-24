/*
 * Wrapper for AMD SysFS on linux, using adapted code from amdcovc by matszpk
 *
 * By Philipp Andreas - github@smurfy.de
 */
#include "pch.hpp"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <string>

#include "AmdSysFsInfoProvider.hpp"
#include "wraphelper.h"

#include "base/cl2.hpp"

static bool getFileContentValue(const char* filename, unsigned int& value)
{
    value = 0;
    std::ifstream ifs(filename, std::ios::binary);
    std::string line;
    std::getline(ifs, line);
    char* p = (char*)line.c_str();
    char* p2;
    errno = 0;
    value = strtoul(p, &p2, 0);
	if (errno != 0) {
		return false;
	}
    return (p != p2);
}

AmdSysFsInfoProvider::AmdSysFsInfoProvider()
{
	memset(&_sysfsh, 0, sizeof(_sysfsh));
}

AmdSysFsInfoProvider::Ref AmdSysFsInfoProvider::create()
{
    Ref provider = new AmdSysFsInfoProvider();

    DIR* dirp = opendir("/sys/class/drm");
	if (dirp == nullptr) {
		return Ref();
	}

    unsigned int gpucount = 0;
    struct dirent* dire;
    errno = 0;
    while ((dire = readdir(dirp)) != nullptr)
    {
		if (::strncmp(dire->d_name, "card", 4) != 0) {
			continue;  // is not card directory
		}
        const char* p;
		for (p = dire->d_name + 4; ::isdigit(*p); p++) {}
		if (*p != 0) {
			continue;  // is not card directory
		}
        unsigned int v = ::strtoul(dire->d_name + 4, nullptr, 10);
        gpucount = std::max(gpucount, v + 1);
    }
    if (errno != 0) {
        closedir(dirp);
		return Ref();
	}
    closedir(dirp);

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
				provider->_gpus.back().extra.id = -1;
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

    // filter AMD GPU cards and create mappings
    char dbuf[120];
    for (unsigned int i = 0; i < gpucount; i++)
    {
        snprintf(dbuf, 120, "/sys/class/drm/card%u/device/vendor", i);
        unsigned int vendorId = 0;
		if (!getFileContentValue(dbuf, vendorId)) {
			continue;
		}
		if (vendorId != 4098) {// if not AMD
			continue;
		}

		// search hwmon
		errno = 0;
		snprintf(dbuf, 120, "/sys/class/drm/card%d/device/hwmon", i);
		DIR* dirp = opendir(dbuf);
		if (dirp == nullptr) {
			return Ref();
		}
		errno = 0;
		struct dirent* dire;
		unsigned int hwmonIndex = UINT_MAX;
		while ((dire = readdir(dirp)) != nullptr)
		{
			if (::strncmp(dire->d_name, "hwmon", 5) != 0) {
				continue;  // is not hwmon directory
			}
			const char* p;
			for (p = dire->d_name + 5; ::isdigit(*p); p++) {}
			if (*p != 0) {
				continue;  // is not hwmon directory
			}
			errno = 0;
			unsigned int v = ::strtoul(dire->d_name + 5, nullptr, 10);
			hwmonIndex = std::min(hwmonIndex, v);
		}
		if (errno != 0) {
			closedir(dirp);
			return Ref();
		}
		closedir(dirp);
		if (hwmonIndex == UINT_MAX) {
			return Ref();
		}

		snprintf(dbuf, 120, "/sys/class/drm/card%d/device/uevent", i);
		std::ifstream ifs(dbuf, std::ios::binary);
		std::string line;
		int iBus = 0, iDevice = 0, iFunction = 0;
		while (std::getline(ifs, line))
		{
			if (line.length() > 24 && line.substr(0, 13) == "PCI_SLOT_NAME")
			{
				std::string pciId = line.substr(14, 12);
				std::vector<std::string> pciParts;
				std::string part;
				std::size_t prev = 0, pos;
				while ((pos = pciId.find_first_of(":.", prev)) != std::string::npos)
				{
					if (pos > prev) {
						pciParts.push_back(pciId.substr(prev, pos - prev));
					}
					prev = pos + 1;
				}
				if (prev < pciId.length()) {
					pciParts.push_back(pciId.substr(prev, std::string::npos));
				}

				// Format -- DDDD:BB:dd.FF
				//??? Above comment doesn't match following statements!!!
				try
				{
					iBus = std::stoul(pciParts[1].c_str());
					iDevice = std::stoul(pciParts[2].c_str());
					iFunction = std::stoul(pciParts[3].c_str());
				}
				catch (...)
				{
					iBus = -1;
					iDevice = -1;
					iFunction = -1;
				}
				break;
			}
		}

		for (auto &clDev : provider->_gpus) {
			if ((iBus == clDev.iBusNumber) && (iDevice == clDev.iDeviceNumber) && (iFunction == clDev.iFunctionNumber)) {
#if 0
				printf("[DEBUG] - SYSFS GPU[%d]%d,%d,%d matches OpenCL GPU[%d]%d,%d,%d\n",
					i,
					iBus,
					iDevice,
					iFunction,
					j, (int)topology.pcie.bus, (int)topology.pcie.device, (int)topology.pcie.function);
#endif
				clDev.iAdapterIndex = i;
				clDev.extra.id = hwmonIndex;
			}
		}
	}

    return provider;
}

bool AmdSysFsInfoProvider::getGpuName(int aGpuIndex, char *namebuf, int bufsize)
{
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size()) {
		return false;
	}

	memcpy(namebuf, _gpus[aGpuIndex].strAdapterName, bufsize);
	return true;
}

bool AmdSysFsInfoProvider::getTempC(int aGpuIndex, unsigned int *tempC)
{
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || -1 == _gpus[aGpuIndex].iAdapterIndex || -1 == _gpus[aGpuIndex].extra.id) {
		return false;
	}

	int gpuindex = _gpus[aGpuIndex].iAdapterIndex;
	int hwmonindex = _gpus[aGpuIndex].extra.id;
	char dbuf[120];
    snprintf(dbuf, 120, "/sys/class/drm/card%d/device/hwmon/hwmon%d/temp1_input", gpuindex, hwmonindex);

    unsigned int temp = 0;
    getFileContentValue(dbuf, temp);

	if (temp > 0) {
		*tempC = temp / 1000;
	}

    return true;
}

bool AmdSysFsInfoProvider::getFanPcnt(int aGpuIndex, unsigned int *fanpcnt)
{
	if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || -1 == _gpus[aGpuIndex].iAdapterIndex || -1 == _gpus[aGpuIndex].extra.id) {
		return false;
	}

	int gpuindex = _gpus[aGpuIndex].iAdapterIndex;
	int hwmonindex = _gpus[aGpuIndex].extra.id;
    unsigned int pwm = 0, pwmMax = 255, pwmMin = 0;

    char dbuf[120];
    snprintf(dbuf, 120, "/sys/class/drm/card%d/device/hwmon/hwmon%d/pwm1", gpuindex, hwmonindex);
    getFileContentValue(dbuf, pwm);

    snprintf(dbuf, 120, "/sys/class/drm/card%d/device/hwmon/hwmon%d/pwm1_max", gpuindex, hwmonindex);
    getFileContentValue(dbuf, pwmMax);

    snprintf(dbuf, 120, "/sys/class/drm/card%d/device/hwmon/hwmon%d/pwm1_min", gpuindex, hwmonindex);
    getFileContentValue(dbuf, pwmMin);

    *fanpcnt = (unsigned int)(double(pwm - pwmMin) / double(pwmMax - pwmMin) * 100.0);
    
	return true;
}

bool AmdSysFsInfoProvider::getPowerUsage(int aGpuIndex, unsigned int *milliwatts)
{
    try
    {
		if (aGpuIndex < 0 || aGpuIndex >= _gpus.size() || -1 == _gpus[aGpuIndex].iAdapterIndex) {
			return false;
		}

		int gpuindex = _gpus[aGpuIndex].iAdapterIndex;

        char dbuf[120];
        snprintf(dbuf, 120, "/sys/kernel/debug/dri/%d/amdgpu_pm_info", gpuindex);

        std::ifstream ifs(dbuf, std::ios::binary);
        std::string line;

        while (std::getline(ifs, line))
        {
            std::smatch sm;
            std::regex regex(R"(([\d|\.]+) W \(average GPU\))");
            if (std::regex_search(line, sm, regex))
            {
                if (sm.size() == 2)
                {
                    double watt = atof(sm.str(1).c_str());
                    *milliwatts = (unsigned int)(watt * 1000);
                    return true;
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
//        std::cwarn << "Error in amdsysfs_get_power_usage: " << ex.what();
    }

    return false;
}
