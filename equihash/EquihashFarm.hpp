#pragma	once

#include "core/Farm.hpp"
#include "Api.hpp"

class EquihashFarm : public core::Farm
{
public:
	EquihashFarm() : _api(*this) {}

	bool Init(int argc, char **argv) override;
	void Cancel() override;

	void PrintStatistics() const;
	void BuildStatistics(std::string &aMsg);

protected:
	Api			_api;
	time_t		_started = 0;
	bool		_checkNoShares = true;
	unsigned	_checkNoSharesTimeout = 120;
	unsigned	_lastSharesCount = 0;
	time_t		_lastSharesTime = 0;
	bool		_useAMD = true;
	bool		_useNvidia = true;
};

