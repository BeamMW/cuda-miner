#include "pch.hpp"
#include "NullWorker.hpp"

using namespace core;

bool NullWorker::Start()
{
	return true;
}

bool NullWorker::Stop()
{
	return true;
}

Work::Ref NullWorker::GetWork()
{
	return Work::Ref();
}

void NullWorker::SetWork(Work::Ref)
{
}

void NullWorker::PostSolution(core::Solution::Ref) const
{
}

bool NullWorker::OnCreateTransport()
{
	return true;
}

bool NullWorker::OnConnected()
{
	return true;
}

bool NullWorker::OnDisconnected()
{
	return true;
}
