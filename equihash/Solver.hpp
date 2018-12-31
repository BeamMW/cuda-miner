#pragma once

#include "BeamWork.hpp"

class Solver : public Dynamic
{
public:
	typedef Reference<Solver> Ref;

	struct Listener
	{
		virtual bool IsCancel(const core::Work &aWork) = 0;
		virtual void OnSolution(const BeamWork &aWork, const std::vector<uint32_t>&, size_t) = 0;
		virtual void OnHashDone() = 0;
	};

public:
	virtual void Solve(BeamWork::Ref aWork, Listener &aListener) = 0;
};

