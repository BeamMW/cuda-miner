#include "pch.hpp"
#include "Farm.hpp"
#include "Service.hpp"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

using namespace core;

Farm::Farm()
{
	srand((unsigned int)time(nullptr));
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

Farm::~Farm()
{
}

bool Farm::Init(int, char **)
{
	return Service::Init();
}

void Farm::Run()
{
	while (core::Work::Ref work = _worker->GetWork()) {
		for (auto miner : _miners) {
			miner->SetWork(work->CreateForThread(miner->GetIndex()));
		}
	}
}

bool Farm::Done()
{
	for (auto miner : _miners) {
		miner->Stop();
	}
	_miners.clear();
	if (_worker) {
		_worker->Stop();
		_worker.release();
	}
	Service::Done();
	return true;
}

void Farm::SetWorker(Worker::Ref aWorker)
{
	_worker = aWorker;
}

