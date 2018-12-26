#pragma	once

#include "core/StratumWorker.hpp"

class EquihashStratumWorker : public core::StratumWorker
{
public:
	EquihashStratumWorker(bool aPoolMode, const std::string &aApiKey);

	void PostSolution(core::Solution::Ref aSolution) const override;

protected:
	bool OnConnected() override;

protected:
	bool		_poolMode;
	std::string	_apiKey;
};
