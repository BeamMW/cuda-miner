#pragma	once

#include <thread>
#include <mutex>
#include "base/TcpServerSocket.hpp"

class Api
{
public:
	Api(class EquihashFarm &aFarm);

	bool Start();
	bool Stop();

protected:
	void Workflow();

public:
	unsigned short		_port = 0;

protected:
	class EquihashFarm	&_farm;
	TcpServerSocket		_apiSocket;
	volatile bool		_exit = false;
	std::thread			_threadWorkflow;
	std::string			_buffer;
};
