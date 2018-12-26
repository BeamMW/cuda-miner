#include "pch.hpp"
#include "base/Endian.hpp"
#include "base/Logging.hpp"
#include "EquihashCudaMiner.hpp"
#include "../EquihashSolution.hpp"
#include "../BeamSolution.hpp"

#if USE_CUDA
EquihashCudaMiner::EquihashCudaMiner(const core::Worker &aWorker, core::CudaMiner::CudaDevice::Ref aCudaDevice)
	: CudaMiner(aCudaDevice)
	, _worker(aWorker)
{
	_statistics.name = _id;
}

bool EquihashCudaMiner::Init()
{
	try {
		_solver = new CudaSolver(_id, _device->_cudaDeviceId);
		LOG(Info) << "GPU:" << _id << ": " << _device->_name;
		return true;
	}
	catch (const std::exception &) {
	}
	return false;
}

unsigned EquihashCudaMiner::Search(const core::Work &aWork)
{
	try {
#if 0
		_solver->Solve(static_cast<const EquihashWork&>(aWork), *this);
#else
		_solver->Solve(static_cast<const BeamWork&>(aWork), *this);
#endif
	}
	catch (const std::exception &) {
	}
	return 1;
}

void EquihashCudaMiner::Search(const core::Work &aWork, Solver::Listener &aListener)
{
	try {
#if 0
		_solver->Solve(static_cast<const EquihashWork&>(aWork), aListener);
#else
		_solver->Solve(static_cast<const BeamWork&>(aWork), aListener);
#endif
	}
	catch (const std::exception &) {
	}
}

void EquihashCudaMiner::Done()
{
	try {
		_solver.release();
	}
	catch (const std::exception &) {
	}
}

bool EquihashCudaMiner::IsCancel(const core::Work &aWork)
{
	return false;
}

void EquihashCudaMiner::OnSolution(const EquihashWork &aWork, const std::vector<uint32_t> &aIndexVector, size_t aCbitlen)
{
	_statistics.solutions.inc();
	if (EquihashSolution::Ref solution = new EquihashSolution(GetId(), aWork, &_statistics)) {
		solution->Add(aIndexVector, aCbitlen);
		if (solution->GetHash() < _worker.GetTarget()) {
			_worker.PostSolution(*solution);
		}
	}
}

void EquihashCudaMiner::OnSolution(const BeamWork &aWork, const std::vector<uint32_t> &aIndexVector, size_t aCbitlen)
{
	_statistics.solutions.inc();
	if (BeamSolution::Ref solution = new BeamSolution(GetId(), aWork, &_statistics)) {
		solution->Add(aIndexVector, aCbitlen);
		beam::uintBig_t<32> hv;
		memcpy(hv.m_pData, solution->GetHash().GetBytes(), 32);
		if (aWork._powDiff.IsTargetReached(hv)) {
			if (_worker.IsCurrentWork(aWork)) {
				_worker.PostSolution(*solution);
			}
		}
	}
}

void EquihashCudaMiner::OnHashDone()
{
	_statistics.hashes.inc();
}

void EquihashCudaMiner::GetMetrics(core::Metrics &aMetrics)
{
	aMetrics.name = _statistics.name;
	aMetrics.accepted = _statistics.accepted;
	aMetrics.fails = _statistics.fails;
	aMetrics.hashes = _statistics.hashes.getTotal();
	aMetrics.hashRateNow = _hashesAvgRate.add(_statistics.hashes);
	aMetrics.hashRateTotal = _statistics.hashes.total();
	aMetrics.rejected = _statistics.rejected;
	aMetrics.solutions = _statistics.solutions.getTotal();
	aMetrics.solutionRateNow = _solutionsAvgRate.add(_statistics.solutions);
	aMetrics.solutionRateTotal = _statistics.solutions.total();
}

void EquihashCudaMiner::GetAvgMetrics(core::Metrics &aMetrics)
{
	aMetrics.name = _statistics.name;
	aMetrics.accepted = _statistics.accepted;
	aMetrics.fails = _statistics.fails;
	aMetrics.hashes = _statistics.hashes.getTotal();
	aMetrics.hashRateNow = _hashesAvgRate.get();
	aMetrics.hashRateTotal = _statistics.hashes.total();
	aMetrics.rejected = _statistics.rejected;
	aMetrics.solutions = _statistics.solutions.getTotal();
	aMetrics.solutionRateNow = _solutionsAvgRate.get();
	aMetrics.solutionRateTotal = _statistics.solutions.total();
}

void EquihashCudaMiner::UpdateStatistics()
{
}

uint32_t EquihashCudaMiner::GetPciBusId() const
{
	return _device ? _device->_pciBusId : -1;
}

uint32_t EquihashCudaMiner::GetPciDeviceId() const
{
	return _device ? _device->_pciDeviceId : -1;
}
#endif
