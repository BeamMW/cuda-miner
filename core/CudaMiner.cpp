#include "pch.hpp"
#include "CudaMiner.hpp"
#include "base/Convertors.hpp"
#include "base/NvidiaInfoProvider.hpp"

#if USE_CUDA
static NvidiaInfoProvider::Ref sNvidiaInfoProvider;

using namespace core;

bool CudaMiner::sIdIsCudaId = true;

CudaMiner::CudaMiner(CudaMiner::CudaDevice::Ref aDevice) : _device(aDevice)
{
	if (_device) {
		if (sIdIsCudaId) {
#if 0
			_id = UintToString(_device->_cudaDeviceId);
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

const char * CudaMiner::GetPlatform() const
{
	return "cuda";
}

bool CudaMiner::IsRunning() const
{
	return true;
}

bool CudaMiner::Start()
{
	_exit = false;
	_threadWorkflow = std::thread(&CudaMiner::Workflow, this);
	return std::thread::id() != _threadWorkflow.get_id();
}

void CudaMiner::Stop()
{
	_exit = true;
	_eventReady.Set();
	_threadWorkflow.join();
	Done();
}

void CudaMiner::SetWork(core::Work::Ref aWork)
{
	if (true) {
		std::unique_lock<std::mutex> lock(_cs);
		_work = aWork;
	}
	if (_work) {
		_eventReady.Set();
	}
}

void CudaMiner::Workflow()
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

bool CudaMiner::GetDevices(CudaDevice::List &aDevices)
{
	if (!sNvidiaInfoProvider) {
		sNvidiaInfoProvider = NvidiaInfoProvider::create();
	}
	if (sNvidiaInfoProvider) {
		for (int id = 0; id < sNvidiaInfoProvider->getGpuCount(); id++) {
			const InfoProvider::AdapterInfo & info = sNvidiaInfoProvider->getAdapterInfo(id);
			char name[128];
			if (sNvidiaInfoProvider->getGpuName(id, name, sizeof(name))) {
				if (CudaDevice::Ref gpu = new CudaDevice()) {
					gpu->_cudaDeviceId = id;
					gpu->_pciBusId = info.iBusNumber;
					gpu->_pciDeviceId = info.iDeviceNumber;
					gpu->_name = name;
					aDevices.push_back(gpu);
				}
			}
		}
		return true;
	}
	return false;
}

void CudaMiner::GetHardwareMetrics(core::HardwareMetrics &aMetrics)
{
	UpdateHardwareStatistics();
	aMetrics = _hardwareMetrics;
}

void CudaMiner::UpdateHardwareStatistics()
{
	if (_device) {
		int id = _device->_cudaDeviceId;
		unsigned value;
		if (sNvidiaInfoProvider->getTempC(id, &value)) {
			_hardwareMetrics.t = value;
		}
		if (sNvidiaInfoProvider->getPowerUsage(id, &value)) {
			_hardwareMetrics.P = value/1000;
		}
		if (sNvidiaInfoProvider->getFanPcnt(id, &value)) {
			_hardwareMetrics.fan = value;
		}
	}
}
#endif
