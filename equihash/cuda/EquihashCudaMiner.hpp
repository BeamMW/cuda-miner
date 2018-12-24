#pragma	once

#if USE_CUDA
#include "core/CudaMiner.hpp"
#include "CudaSolver.hpp"
#include "core/Worker.hpp"

class EquihashCudaMiner : public core::CudaMiner, Solver::Listener
{
public:
	typedef Reference<EquihashCudaMiner> Ref;

public:
	EquihashCudaMiner(const core::Worker &aWorker, core::CudaMiner::CudaDevice::Ref aCudaDevice);

	bool Init() override;
	unsigned Search(const core::Work &aWork) override;
	void Done() override;

	uint32_t GetPciBusId() const override;
	uint32_t GetPciDeviceId() const override;

	void GetAvgMetrics(core::Metrics &aMetrics) override;
	void GetMetrics(core::Metrics &aMetrics) override;
	void UpdateStatistics() override;

	void Search(const core::Work &aWork, Solver::Listener &aListener);

protected:
	bool IsCancel(const core::Work &aWork) override;
	void OnSolution(const EquihashWork &aWork, const std::vector<uint32_t>&, size_t) override;
	void OnHashDone() override;

protected:
	const core::Worker	&_worker;
	Solver::Ref			_solver;
};
#endif
