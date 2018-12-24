#include "pch.hpp"
#include "Miner.hpp"

using namespace core;

static uint32_t sMinerIndex = 0;

Miner::Miner()
{
	_index = sMinerIndex++;
}

uint32_t Miner::GetIndex() const
{
	return _index;
}

const std::string & Miner::GetId() const
{
	return _id;
}

const core::Statistics & Miner::GetStatistics() const
{
	return _statistics;
}
