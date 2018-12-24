#include "pch.hpp"
#include <chrono>
#include "base/Logging.hpp"
#include "EquihashFarm.hpp"

Api::Api(class EquihashFarm &aFarm) : _farm(aFarm)
{
}

bool Api::Start()
{
	if (_port) {
		_exit = false;
		if (!_apiSocket.Bind("127.0.0.1", _port)) {
			return false;
		}
		if (!_apiSocket.Listen()) {
			return false;
		}
		_threadWorkflow = std::thread(&Api::Workflow, this);
		if (std::thread::id() != _threadWorkflow.get_id()) {
			return true;
		}
	}
	_exit = true;
	return false;
}

bool Api::Stop()
{
	_exit = true;
	_apiSocket.Close();
	if (_threadWorkflow.native_handle() && _threadWorkflow.joinable()) {
		try {
			_threadWorkflow.join();
		}
		catch (...) {
		}
	}
	return true;
}

void Api::Workflow()
{
	while (!_exit) {
		TcpClientSocket client(_apiSocket.Accept());
		if (!client.IsInvalid() && client.IsConnected()) {
			std::string msg;
			_farm.BuildStatistics(msg);
			LOG(Trace) << "API: " << msg;
			client.Send(msg.c_str(), msg.length());
			client.Close();
		}
	}
}

