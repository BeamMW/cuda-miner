#pragma	once

#if USE_CUDA
#include <thread>
#include <mutex>
#include "base/Signal.hpp"
#include "core/Miner.hpp"

namespace core
{
	class CudaMiner : public Miner
	{
	public:
		typedef Reference<CudaMiner> Ref;

		class CudaDevice : public Dynamic
		{
		public:
			typedef Reference<CudaDevice> Ref;
			typedef std::list<Ref> List;

		public:
			uint32_t	_cudaDeviceId;
			uint32_t	_pciBusId;
			uint32_t	_pciDeviceId;
			std::string	_name;
		};

	public:
		CudaMiner(CudaDevice::Ref aDevice);

		const char * GetPlatform() const override;
		bool IsRunning() const override;

		bool Start() override;
		void Stop() override;

		void GetHardwareMetrics(core::HardwareMetrics &aMetrics) override;
		void UpdateHardwareStatistics() override;

		void SetWork(core::Work::Ref aWork) override;

		static bool GetDevices(CudaDevice::List &aDevices);

	protected:
		virtual bool Init() = 0;
		virtual unsigned Search(const core::Work &aWork) = 0;
		virtual void Done() = 0;
		void Workflow();

	public:
		static bool sIdIsCudaId;

	protected:
		volatile bool	_exit = false;
		std::thread		_threadWorkflow;
		std::mutex		_cs;
		CudaDevice::Ref	_device;
		core::Work::Ref	_work;
		PulseSignal		_eventReady;
		core::HardwareMetrics	_hardwareMetrics;
	};
}
#endif
