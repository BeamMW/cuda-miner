#include "pch.hpp"
#include "ClMiner.hpp"
#include "base/Convertors.hpp"
#include "base/AdlInfoProvider.hpp"

#if USE_OPENCL
#include <CL/cl_ext.h>
#ifndef _WIN32
#include "base/AmdSysFsInfoProvider.hpp"
#endif
static InfoProvider::Ref sInfoProvider;

using namespace core;

bool ClMiner::sIdIsClId = true;

ClMiner::ClMiner(ClMiner::ClDevice::Ref aDevice) : _device(aDevice)
{
	if (_device) {
		if (sIdIsClId) {
#if 0
			_id = UintToString(_device->_id);
#else
			_id = UintToString(_index);
#endif
		}
		else {
			_id = std::to_string(_device->_pciBusId);
			_id += ':';
			_id += std::to_string(_device->_pciDeviceId);
		}
	}
}

const char * ClMiner::GetPlatform() const
{
	return "cl";
}

bool ClMiner::IsRunning() const
{
	return true;
}

bool ClMiner::Start()
{
	_exit = false;
	_threadWorkflow = std::thread(&ClMiner::Workflow, this);
	return std::thread::id() != _threadWorkflow.get_id();
}

void ClMiner::Stop()
{
	_exit = true;
	_eventReady.Set();
	_threadWorkflow.join();
	Done();
}

void ClMiner::SetWork(core::Work::Ref aWork)
{
	if (true) {
		std::unique_lock<std::mutex> lock(_cs);
		_work = aWork;
	}
	if (_work) {
		_eventReady.Set();
	}
}

void ClMiner::Workflow()
{
	if (Init()) {
		core::Work::Ref current;
		while (!_exit) {
			if (_work || _eventReady.Wait(INFINITE)) {
				if (_exit) {
					break;
				}
				if (_work.get() != current.get()) {
					current = _work;
				}
			}
			unsigned inc = Search(*current);
			if (_exit || -1 == inc) {
				break;
			}
			current->Increment(inc);
		}
		Done();
	}
}

bool ClMiner::GetDevices(ClDevice::List &aDevices)
{
	if (!sInfoProvider) {
		sInfoProvider = AdlInfoProvider::create().get();
#ifndef _WIN32
		if (!sInfoProvider) {
			sInfoProvider = AmdSysFsInfoProvider::create().get();
		}
#endif
	}

	if (sInfoProvider) {
		std::vector<cl::Platform> platforms;
		if (CL_SUCCESS == cl::Platform::get(&platforms)) {
			if (platforms.size() >= 1) {
				for (int platformId = 0; platformId < platforms.size(); platformId++) {
					std::vector<cl::Device> devices;
					if (CL_SUCCESS == platforms[platformId].getDevices(CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR, &devices)) {
						cl_device_topology_amd topology;
						for (int id = 0; id < devices.size(); id++) {
							try {
								if (CL_SUCCESS == devices[id].getInfo(CL_DEVICE_TOPOLOGY_AMD, &topology)) {
									if (topology.raw.type == CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD) {
										if (ClDevice::Ref gpu = new ClDevice()) {
											gpu->_device = devices[id];
											gpu->_pciBusId = topology.pcie.bus;
											gpu->_pciDeviceId = topology.pcie.device;
											aDevices.push_back(gpu);
										}
									}
								}
							}
							catch (cl::Error const&) {
							}
						}
					}
				}
			}
		}

		for (int id = 0; id < sInfoProvider->getGpuCount(); id++) {
			const InfoProvider::AdapterInfo & info = sInfoProvider->getAdapterInfo(id);
			for (auto gpu : aDevices) {
				if (gpu->_pciBusId == info.iBusNumber && gpu->_pciDeviceId == info.iDeviceNumber) {
					char name[128];
					if (sInfoProvider->getGpuName(id, name, sizeof(name))) {
						gpu->_id = id;
						gpu->_name = name;
					}
				}
			}
		}

		return true;
	}

	return false;
}

void ClMiner::GetHardwareMetrics(core::HardwareMetrics &aMetrics)
{
	UpdateHardwareStatistics();
	aMetrics = _hardwareMetrics;
}

void ClMiner::UpdateHardwareStatistics()
{
	if (_device) {
		unsigned value;
		if (sInfoProvider->getTempC(_device->_id, &value)) {
			_hardwareMetrics.t = value;
		}
		if (sInfoProvider->getPowerUsage(_device->_id, &value)) {
			_hardwareMetrics.P = value/1000;
		}
		if (sInfoProvider->getFanPcnt(_device->_id, &value)) {
			_hardwareMetrics.fan = value;
		}
	}
}
#endif
