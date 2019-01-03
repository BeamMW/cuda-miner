#include "pch.hpp"
#include <chrono>
#include "StratumWorker.hpp"
#include "base/Logging.hpp"
#include "core/TcpAsyncTransport.hpp"
#include "core/SecureTcpTransport.hpp"

using namespace core;

const std::string & StratumWorker::Call::GetId() const
{
	return _id;
}

const std::string & StratumWorker::Call::GetName() const
{
	return _name;
}

bool StratumWorker::Call::OnCall(const JSonVar &)
{
	return false;
}

bool StratumWorker::Call::OnResult(const JSonVar &)
{
	return false;
}

void StratumWorker::Call::OnTimeout()
{
}

bool StratumWorker::Call::HasResult() const
{
	return false;
}

bool StratumWorker::Call::HasParams() const
{
	return true;
}

void StratumWorker::Call::Serialize(std::string &) const
{
}

//=========================================================

const std::string StratumWorker::kError("error");
const std::string StratumWorker::kId("id");
const std::string StratumWorker::kMethod("method");
const std::string StratumWorker::kParams("params");
const std::string StratumWorker::kResult("result");

StratumWorker::StratumWorker()
	: _callId(0)
{
}

bool StratumWorker::Start()
{
	_exit = false;
	_eventSync.Reset();
	_threadWorkflow = std::thread(&StratumWorker::Workflow, this);
	if (std::thread::id() != _threadWorkflow.get_id()) {
		_threadSender = std::thread(&StratumWorker::Sender, this);
		if (std::thread::id() != _threadWorkflow.get_id()) {
			_eventSync.Set();
			return true;
		}
		_exit = true;
		_eventSync.Set();
		_threadWorkflow.join();
	}
	return false;
}

bool StratumWorker::Stop()
{
	_exit = true;
	_eventSync.Set();
	_eventWork.Set();
	if (_transport) {
		_transport->Close();
	}
	if (_threadWorkflow.native_handle() && _threadWorkflow.joinable()) {
		try {
			_threadWorkflow.join();
		}
		catch (...) {
		}
	}
	if (_threadSender.native_handle() && _threadSender.joinable()) {
		try {
			_threadSender.join();
		}
		catch (...) {
		}
	}
	_transport.release();
	return true;
}

bool StratumWorker::Register(StratumWorker::Call::Ref aCall)
{
	return aCall && _callHandlers.insert(Call::Map::value_type(aCall->GetName(), aCall)).second;
}

void StratumWorker::Workflow()
{
	JSonVar pt;

	_eventSync.Wait(INFINITE);

	if (!OnCreateTransport()) {
		LOG(Error) << "Create Transport error";
		return;
	}

	while (!_exit) {
		try {
			LOG(Info) << "Connecting to " << _server << ":" << _port << " ...";
			if (_transport->Open()) {
				LOG(Info) << "Connected to " << _server << ":" << _port;
				if (OnConnected()) {
					try {
						while (ReadJson(pt)) {
							if (_exit) {
								break;
							}
							if (auto id = pt[kId]) {
								if (id->IsNull()) {
									if (auto method = pt[kMethod]) {
										OnNotify(method->GetValue(), pt);
									}
									else {
										//
									}
								}
								else {
									if (pt.Exists(kResult)) {
										OnResult(id->GetValue(), pt);
									}
									else {
										if (auto method = pt[kMethod]) {
#if 0
											OnCall(id->GetValue(), method->GetValue(), pt);
#else
											OnNotify(method->GetValue(), pt);
#endif
										}
										else {
											//
										}
									}
								}
							}
						}
					}
					catch (std::exception& e) {
						LOG(Error) << "Exception in " << __FUNCTION__ << ": " << e.what();
					}
					_transport->Close();
					OnDisconnected();
				}
			}
		}
		catch (std::exception& e) {
			LOG(Error) << "Exception in " << __FUNCTION__ << ": " << e.what();
		}
		if (_exit) {
			break;
		}
		LOG(Info) << "Reconnect in 15 s";
		std::this_thread::sleep_for(std::chrono::seconds(15));
	}
}

void StratumWorker::Sender()
{
	std::string buffer;

	_eventSync.Wait(INFINITE);

	while (!_exit) {
		if (_semAvailable.wait()) {
			Call::Ref method;
			{
				std::unique_lock<std::mutex> lock(_csOutput);
				if (!_outputCalls.empty()) {
					method = _outputCalls.front();
					_outputCalls.pop_front();
				}
			}
			if (_exit) {
				break;
			}
			if (method) {
				buffer.clear();
				method->Serialize(buffer);
				if (_exit) {
					break;
				}
				if (!buffer.empty()) {
					if (_transport && _transport->IsReady()) {
						bool inQueue = false;
						if (method->HasResult() && !method->GetId().empty()) {
							std::unique_lock<std::mutex> lock(_csOutput);
							inQueue = _pendingCalls.insert(Call::Map::value_type(method->GetId(), method)).second;
						}
						LOG(Trace) << buffer;
						int result = _transport->PutLine(buffer);
						if (inQueue && (result <= 0)) {
							std::unique_lock<std::mutex> lock(_csOutput);
							_pendingCalls.erase(method->GetId());
						}
					}
				}
			}
		}
	}
}

bool StratumWorker::RemoteCall(Call::Ref aCall)
{
	std::unique_lock<std::mutex> lock(_csOutput);
	_outputCalls.push_back(aCall);
	_semAvailable.unlock();
	return true;
}

bool StratumWorker::OnCall(const std::string &, const std::string &, const JSonVar &)
{
	return false;
}

bool StratumWorker::OnNotify(const std::string &aName, const JSonVar &aCall)
{
	auto handler = _callHandlers.find(aName);
	if (_callHandlers.end() != handler) {
		if (handler->second->HasParams()) {
			if (auto params = aCall[kParams]) {
				handler->second->OnCall(*params);
			}
		}
		else {
			handler->second->OnCall(aCall);
		}
		return true;
	}
	return false;
}

bool StratumWorker::OnResult(const std::string &aId, const JSonVar &aCall)
{
	Call::Ref method;
	if (true) {
		std::unique_lock<std::mutex> lock(_csOutput);
		auto m = _pendingCalls.find(aId);
		if (_pendingCalls.end() != m) {
			method = m->second;
			_pendingCalls.erase(m);
		}
	}
	if (method) {
		method->OnResult(aCall);
	}
	return true;
}

bool StratumWorker::ReadJson(JSonVar &aReply)
{
	aReply.Clear();
	_buffer.clear();
	try {
		while (!_exit) {
			int result = _transport->GetLine(_buffer, 5 * 60 * 1000);
			if (0 == result) {
				LOG(Trace) << _buffer;
				return JSonVar::ParseJSon(_buffer.c_str(), aReply);
			}
			else {
#if 0
				if (-2 == result) { // Timeout
					continue;
				}
#endif
				LOG(Error) << "Socket read error in " << __FUNCTION__ << ": " << result;
				return false;
			}
		}
	}
	catch (std::exception& e) {
		LOG(Error) << "Exception in " << __FUNCTION__ << ": " << e.what();
	}
	return false;
}

bool StratumWorker::OnDisconnected()
{
	_pendingCalls.clear();
	std::unique_lock<std::mutex> lock(_csOutput);
	_outputCalls.clear();
	_semAvailable.unlock();
	_work.release();
	_eventWork.Set();
	return true;
}

bool StratumWorker::OnCreateTransport()
{
#if 0
	_transport = new TcpAsyncTransport(_server, atoi(_port.c_str()));
#else
	_transport = new SecureTcpTransport(_server, atoi(_port.c_str()));
#endif
	return _transport;
}

Work::Ref StratumWorker::GetWork()
{
	if (_eventWork.Wait(INFINITE)) {
		if (!_exit) {
			return _work;
		}
	}
	return Work::Ref();
}

void StratumWorker::SetWork(Work::Ref aWork)
{
	if (aWork) {
		_workCounter++;
	}
	_work = aWork;
	_eventWork.Set();
}

void StratumWorker::CancelWork(const std::string &aId)
{
	if (_work && (_work->_id == aId)) {
		_work.release();
		_eventWork.Set();
	}
}

bool StratumWorker::IsCurrentWork(const core::Work &aWork) const
{
	return _work && (_work->_id == aWork._id);
}

long StratumWorker::CreateCallId()
{
	return _callId++;
}

