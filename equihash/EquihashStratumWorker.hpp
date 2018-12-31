#pragma	once

#include "core/StratumWorker.hpp"

class EquihashStratumWorker : public core::StratumWorker
{
public:
	EquihashStratumWorker(const std::string &aApiKey);

	void PostSolution(core::Solution::Ref aSolution) const override;

protected:
	bool OnConnected() override;

protected:
	std::string	_apiKey;
};
