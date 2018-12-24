#pragma	once

#if USE_OPENCL
#include <thread>
#include <mutex>
#include "base/Signal.hpp"
#include "core/Miner.hpp"

#include "base/cl2.hpp"

namespace core
{
	class ClMiner : public Miner
	{
	public:
		typedef Reference<ClMiner> Ref;

		class ClDevice : public Dynamic
		{
		public:
			typedef Reference<ClDevice> Ref;
			typedef std::list<Ref> List;

		public:
			cl::Device	_device;
			uint32_t	_id;
			uint32_t	_pciBusId;
			uint32_t	_pciDeviceId;
			std::string	_name;
		};

	public:
		ClMiner(ClDevice::Ref aDevice);

		const char * GetPlatform() const override;
		bool IsRunning() const override;

		bool Start() override;
		void Stop() override;

		void GetHardwareMetrics(core::HardwareMetrics &aMetrics) override;
		void UpdateHardwareStatistics() override;

		void SetWork(core::Work::Ref aWork) override;

		static bool GetDevices(ClDevice::List &aDevices);

	protected:
		virtual bool Init() = 0;
		virtual unsigned Search(const core::Work &aWork) = 0;
		virtual void Done() = 0;
		void Workflow();
	
	public:
		static bool sIdIsClId;

	protected:
		volatile bool	_exit = false;
		std::thread		_threadWorkflow;
		std::mutex		_cs;
		ClDevice::Ref	_device;
		core::Work::Ref	_work;
		PulseSignal		_eventReady;
		core::HardwareMetrics	_hardwareMetrics;
	};
}
#endif
