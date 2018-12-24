#pragma	once

#include "core/StratumWorker.hpp"

class EquihashStratumWorker : public core::StratumWorker
{
public:
	EquihashStratumWorker();

	void PostSolution(core::Solution::Ref aSolution) const override;

protected:
	bool OnConnected() override;
};
