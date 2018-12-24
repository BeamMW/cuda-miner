#pragma	once

#include "core/Statistics.hpp"
#include "core/Work.hpp"
#include "core/HardwareMetrics.hpp"

namespace core
{
	class Miner : public Dynamic
	{
	public:
		static const int cAverageLength = 6;

	public:
		typedef Reference<Miner> Ref;
		typedef std::list<Ref> List;

	public:
		Miner();

		virtual uint32_t GetIndex() const;
		virtual const std::string & GetId() const;
		virtual const char * GetPlatform() const = 0;
		virtual bool IsRunning() const = 0;

		virtual uint32_t GetPciBusId() const = 0;
		virtual uint32_t GetPciDeviceId() const = 0;

		virtual void GetAvgMetrics(core::Metrics &aMetrics) = 0;
		virtual void GetMetrics(core::Metrics &aMetrics) = 0;
		virtual void UpdateStatistics() = 0;
		virtual void GetHardwareMetrics(core::HardwareMetrics &aMetrics) = 0;
		virtual void UpdateHardwareStatistics() = 0;
		virtual const core::Statistics & GetStatistics() const;

		virtual bool Start() = 0;
		virtual void Stop() = 0;

		virtual void SetWork(core::Work::Ref aWork) = 0;

	protected:
		std::string			_id;
		uint32_t			_index = 0;
		bool				_running = false;
		core::Statistics	_statistics;
		core::Average<cAverageLength>	_hashesAvgRate;
		core::Average<cAverageLength>	_solutionsAvgRate;
	};
}
