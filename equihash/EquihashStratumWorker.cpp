#include "pch.hpp"
#include "base/BigInteger.hpp"
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/Logging.hpp"
#include "EquihashStratumWorker.hpp"
#include "EquihashWork.hpp"
#include "BeamWork.hpp"
#include "EquihashSolution.hpp"

class MiningAuthorizeCall : public core::StratumWorker::Call
{
public:
	MiningAuthorizeCall(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.authorize";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": 2, "result": true, "error": null}\n
		if (auto error = aResult[core::StratumWorker::kError]) {
			if (error->IsNull()) {
				if (auto result = aResult[core::StratumWorker::kResult]) {
					result->GetBoolValue();
				}
			}
		}
		else {
		}
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		// {"id": 2, "method": "mining.authorize", "params": ["WORKER_NAME", "WORKER_PASSWORD"]}\n
		aBuffer += "{\"id\":";
		aBuffer += _id;
		aBuffer += ",\"method\":\"mining.authorize\",\"params\":[\"";
		aBuffer += _worker.GetUser();
		aBuffer += ".";
		aBuffer += _worker.GetWorker();
		aBuffer += "\",\"";
		aBuffer += _worker.GetPassword();
		aBuffer += "\"]}\n";
	}

protected:
	core::StratumWorker	&_worker;
};

class MiningSubscribeExtranonceCall : public core::StratumWorker::Call
{
public:
	MiningSubscribeExtranonceCall(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.subscribe.extranonce";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": X, "result": true, "error": null}\n
		if (auto error = aResult[core::StratumWorker::kError]) {
			if (error->IsNull()) {
			}
			else {
			}
		}
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		// {"id": X, "method": "mining.subscribe.extranonce", "params": []}\n
		aBuffer += "{\"id\":";
		aBuffer += _id;
		aBuffer += ",\"method\":\"mining.subscribe.extranonce\",\"params\":[]}\n";
	}

protected:
	core::StratumWorker	&_worker;
};

class MiningSubscribeCall : public core::StratumWorker::Call
{
public:
	MiningSubscribeCall(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.subscribe";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": 1, "result": ["SESSION_ID", "NONCE_1"], "error": null}\n
		if (auto error = aResult[core::StratumWorker::kError]) {
			if (error->IsNull()) {
				if (auto result = aResult[core::StratumWorker::kResult]) {
					if (result->IsArray() && result->GetArrayLen() == 2) {
						std::string value = result->GetByIndex(0)->GetValue();
						std::string nonce_1 = result->GetByIndex(1)->GetValue();
						if (!nonce_1.empty()) {
							BigInteger extraNonce;
							if (extraNonce.Import(nonce_1, false)) {
								_worker.SetExtraNonce(extraNonce);
							}
							else {
								// TODO: Error report
								return false;
							}
							_worker.RemoteCall(new MiningAuthorizeCall(_worker));
							_worker.RemoteCall(new MiningSubscribeExtranonceCall(_worker));
						}
						else {
							// TODO: Error report
							return false;
						}
					}
					else {
						// TODO: Error report
						return false;
					}
				}
				else {
					// TODO: Error report
					return false;
				}
			}
			else {
				// TODO: Error report
				return false;
			}
		}
		else {
			// TODO: Error report
			return false;
		}
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		// {"id": 1, "method": "mining.subscribe", "params": ["CONNECT_HOST", CONNECT_PORT, "MINER_USER_AGENT", "SESSION_ID"]}\n
		aBuffer += "{\"id\":";
		aBuffer += _id;
		aBuffer += ",\"method\":\"mining.subscribe\",\"params\":[\"";
		aBuffer += _worker.GetServer();
		aBuffer += "\",";
		aBuffer += _worker.GetPort();
		aBuffer += ",\"minerall-equihash-miner/1.0\",null]}\n";
	}

protected:
	core::StratumWorker	&_worker;
};

class MiningSetTargetNotify : public core::StratumWorker::Call
{
public:
	MiningSetTargetNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "mining.set_target";
	}

	bool OnCall(const JSonVar &aParameters) override
	{
		// {"id": null, "method": "mining.set_target", "params": ["TARGET"]}\n
		if (aParameters.IsArray() && aParameters.GetArrayLen() == 1) {
			std::string value = aParameters.GetByIndex(0)->GetValue();
			if (!value.empty()) {
				BigInteger target;
				if (target.Import(value, true)) {
					_worker.SetTarget(target);
					LOG(Info) << "Set target to " << value;
				}
				else {
					// TODO: Error report
					return false;
				}
			}
			else {
				// TODO: Error report
				return false;
			}
		}
		else {
			// TODO: Error report
			return false;
		}
		return true;
	}

protected:
	core::StratumWorker	&_worker;
};

class MiningSetExtranonceNotify : public core::StratumWorker::Call
{
public:
	MiningSetExtranonceNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "mining.set_extranonce";
	}

	bool OnCall(const JSonVar &aParameters) override
	{
		// {"id": null, "method": "mining.set_extranonce", "params": ["NONCE1", NONCE_LENGTH]}\n
		if (aParameters.IsArray() && 2 == aParameters.GetArrayLen()) {
			std::string nonce = aParameters.GetByIndex(0)->GetValue();
			if (!nonce.empty()) {
				if (size_t length = std::stoi(aParameters.GetByIndex(1)->GetValue())) {
					BigInteger nonce_1;
					if (nonce_1.Import(nonce, false) && nonce_1.GetBytesLength() == length) {
						_worker.SetExtraNonce(nonce_1);
						LOG(Info) << "Set extranonce to " << nonce;
					}
					else {
						// TODO: Error report
						return false;
					}
				}
				else {
					// TODO: Error report
					return false;
				}
			}
			else {
				// TODO: Error report
				return false;
			}
		}
		else {
			// TODO: Error report
			return false;
		}
		return true;
	}

protected:
	core::StratumWorker	&_worker;
};

class MiningNotifyNotify : public core::StratumWorker::Call
{
public:
	MiningNotifyNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "mining.notify";
	}

	bool OnCall(const JSonVar &aParameters) override
	{
		// {"id": null, "method": "mining.notify", "params": ["JOB_ID", "VERSION", "PREVHASH", "MERKLEROOT", "RESERVED", "TIME", "BITS", CLEAN_JOBS]}\n
		if (aParameters.IsArray() && 8 == aParameters.GetArrayLen()) {
			if (EquihashWork::Ref work = new EquihashWork()) {
				work->_id = aParameters.GetByIndex(0)->GetValue(); // "JOB_ID"
				if (work->_id.empty()) {
					LOG(Error) << "Wrong mining.notify JOB_ID";
					return false;
				}
				std::string value = aParameters.GetByIndex(1)->GetValue(); // "VERSION"
				if (!value.empty()) {
					unsigned char *data = work->GetData();
					int result = HexToBin(value, data, 4);
					if (4 == result) {
						work->_version = *(uint32_t*)data;
						if (4 != work->_version) {
							LOG(Error) << "Wrong mining.notify VERSION: " << work->_version;
							return false;
						}
						data += 4;
						value = aParameters.GetByIndex(2)->GetValue(); // "PREVHASH"
						if (!value.empty()) {
							int result = HexToBin(value, data, 32);
							if (32 != result) {
								LOG(Error) << "Wrong mining.notify PREVHASH length: " << result;
								return false;
							}
							data += 32;
						}
						else {
							LOG(Error) << "Wrong mining.notify PREVHASH";
							return false;
						}
						value = aParameters.GetByIndex(3)->GetValue(); // "MERKLEROOT"
						if (!value.empty()) {
							int result = HexToBin(value, data, 32);
							if (32 != result) {
								LOG(Error) << "Wrong mining.notify MERKLEROOT length: " << result;
								return false;
							}
							data += 32;
						}
						else {
							LOG(Error) << "Wrong mining.notify MERKLEROOT";
							return false;
						}
						value = aParameters.GetByIndex(4)->GetValue(); // "RESERVED"
						if (!value.empty()) {
							int result = HexToBin(value, data, 32);
							if (32 != result) {
								LOG(Error) << "Wrong mining.notify RESERVED length: " << result;
								return false;
							}
							data += 32;
						}
						else {
							LOG(Error) << "Wrong mining.notify RESERVED";
							return false;
						}
						value = aParameters.GetByIndex(5)->GetValue(); // "TIME"
						if (!value.empty()) {
							int result = HexToBin(value, (unsigned char*)&work->_time, 4);
							if (4 != result) {
								LOG(Error) << "Wrong mining.notify TIME length: " << result;
								return false;
							}
							*(uint32_t*)data = work->_time;
							data += 4;
						}
						else {
							LOG(Error) << "Wrong mining.notify TIME";
							return false;
						}
						value = aParameters.GetByIndex(6)->GetValue(); // "BITS"
						if (!value.empty()) {
							int result = HexToBin(value, (unsigned char*)&work->_nbits, 4);
							if (4 != result) {
								LOG(Error) << "Wrong mining.notify BITS length: " << result;
								return false;
							}
							*(uint32_t*)data = work->_nbits;
							data += 4;
						}
						else {
							LOG(Error) << "Wrong mining.notify BITS";
							return false;
						}
						work->_clean = aParameters.GetByIndex(7)->GetBoolValue(); // CLEAN_JOBS

						work->SetExtraNonceSize((uint32_t)_worker.GetExtraNonce().GetBytesLength());
						memcpy(work->GetNonce(), _worker.GetExtraNonce().GetBytes(), _worker.GetExtraNonce().GetBytesLength());
						memset(work->GetNonce() + _worker.GetExtraNonce().GetBytesLength(), 0, work->GetNonceSize() - _worker.GetExtraNonce().GetBytesLength());
						LOG(Info) << "New job " << work->_id;
						_worker.SetWork(*work);
						return true;
					}
					else {
						LOG(Error) << "Wrong mining.notify VERSION length: " << result;
						return false;
					}
				}
				else {
					LOG(Error) << "Wrong mining.notify VERSION";
					return false;
				}
			}
		}
		else {
			LOG(Error) << "Wrong mining.notify params count: " << aParameters.GetArrayLen();
		}
		return false;
	}

protected:
	core::StratumWorker	&_worker;
};

class MiningSubmitCall : public core::StratumWorker::Call
{
public:
	MiningSubmitCall(core::StratumWorker &aWorker, core::Solution::Ref aSolution) : _worker(aWorker), _solution(aSolution)
	{
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.submit";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": 4, "result": true, "error": null}\n
		if (auto result = aResult[core::StratumWorker::kResult]) {
			if (result->GetBoolValue()) {
				_solution->OnAccepted((unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count());
			}
			else {
				std::string reason;
				if (auto error = aResult[core::StratumWorker::kError]) {
				}
				_solution->OnRejected((unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count(), reason);
			}
		}
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		// {"id": 4, "method": "mining.submit", "params": ["WORKER_NAME", "JOB_ID", "TIME", "NONCE_2", "EQUIHASH_SOLUTION"]}\n
		aBuffer += "{\"id\":";
		aBuffer += _id;
		aBuffer += ",\"method\":\"mining.submit\",\"params\":[\"";
		aBuffer += _worker.GetUser();
		aBuffer += ".";
		aBuffer += _worker.GetWorker();
		aBuffer += "\",\"";
		aBuffer += _solution->GetWorkId();
		aBuffer += "\",\"";
		_solution->PrintTime(aBuffer);
		aBuffer += "\",\"";
		_solution->PrintNonce(aBuffer, false);
		aBuffer += "\",\"";
		_solution->PrintSolution(aBuffer);
		aBuffer += "\"]}\n";
	}

protected:
	core::StratumWorker		&_worker;
	core::Solution::Ref		_solution;
	core::Performance::Time	_start = std::chrono::system_clock::now();
};

// Node methods

class LoginCall : public core::StratumWorker::Call
{
public:
	LoginCall(core::StratumWorker &aWorker, const std::string &aApiKey) : _worker(aWorker), _apiKey(aApiKey)
	{
		_id = "login";
		_name = "login";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": 2, "result": true, "error": null}\n
		if (auto error = aResult[core::StratumWorker::kError]) {
			if (error->IsNull()) {
				if (auto result = aResult[core::StratumWorker::kResult]) {
					result->GetBoolValue();
				}
			}
		}
		else {
		}
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		// {"method":"login", "api_key":"", "id":"login","jsonrpc":"2.0"}\n
		aBuffer += "{\"method\":\"login\",\"api_key\":\"";
		aBuffer += _apiKey;
		aBuffer += "\",\"id\":\"login\",\"jsonrpc\":\"2.0\"}\n";
	}

protected:
	core::StratumWorker	&_worker;
	std::string			_apiKey;
};

class CancelNotify : public core::StratumWorker::Call
{
public:
	CancelNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "cancel";
	}

	bool OnCall(const JSonVar &aCall) override
	{
		// {"jsonrpc":"2.0", "id": id, "method": "cancel"}\n
		if (auto id = aCall[core::StratumWorker::kId]) {
			_worker.CancelWork(id->GetValue());
		}
		return true;
	}

	bool HasParams() const override
	{
		return false;
	}

protected:
	core::StratumWorker	&_worker;
};

class JobNotify : public core::StratumWorker::Call
{
public:
	JobNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "job";
	}

	bool OnCall(const JSonVar &aCall) override
	{
		static const std::string kInput("input");
		static const std::string kDifficulty("difficulty");

		// {"jsonrpc":"2.0", "id": id, "method": "job"}\n
		if (BeamWork::Ref work = new BeamWork()) {
			if (auto id = aCall[core::StratumWorker::kId]) {
				work->_id = id->GetValue();
				if (auto input = aCall[kInput]) {
					work->_input.Import(input->GetValue(), true);
					if (auto difficulty = aCall[kDifficulty]) {
						work->_powDiff = beam::Difficulty(difficulty->GetULongValue());
						LOG(Info) << "New job " << work->_id;
						_worker.SetWork(*work);
						return true;
					}
				}
			}
		}
		return true;
	}

	bool HasParams() const override
	{
		return false;
	}

protected:
	core::StratumWorker	&_worker;
};

class SolutionCall : public core::StratumWorker::Call
{
public:
	SolutionCall(core::StratumWorker &aWorker, core::Solution::Ref aSolution) : _worker(aWorker), _solution(aSolution)
	{
		_id = _solution->GetWorkId();
		_name = "solution";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": 4, "result": true, "error": null}\n
		if (auto result = aResult[core::StratumWorker::kResult]) {
			if (result->GetBoolValue()) {
				_solution->OnAccepted((unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count());
			}
			else {
				std::string reason;
				if (auto error = aResult[core::StratumWorker::kError]) {
				}
				_solution->OnRejected((unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count(), reason);
			}
		}
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		// 	{\"method\" : \"solution\", \"id\": \"id\", \"nonce\": \"nonce\", \"output\": \"solution\", \"jsonrpc\":\"2.0\" } \n;
		
		aBuffer += "{\"method\":\"solution\",\"id\":\"";
		aBuffer += _id;
		aBuffer += "\",\"nonce\":\"";
		_solution->PrintNonce(aBuffer, false);
		aBuffer += "\",\"output\":\"";
		_solution->PrintSolution(aBuffer);
		aBuffer += "\",\"jsonrpc\":\"2.0\"}\n";
	}

protected:
	core::StratumWorker		&_worker;
	core::Solution::Ref		_solution;
	core::Performance::Time	_start = std::chrono::system_clock::now();
};

//

EquihashStratumWorker::EquihashStratumWorker(bool aPoolMode, const std::string &aApiKey) : _poolMode(aPoolMode), _apiKey(aApiKey)
{
	if (_poolMode) {
		Register(new MiningSetTargetNotify(*this));
		Register(new MiningSetExtranonceNotify(*this));
		Register(new MiningNotifyNotify(*this));
	}
	else {
		Register(new JobNotify(*this));
		Register(new CancelNotify(*this));
	}
}

bool EquihashStratumWorker::OnConnected()
{
	if (_poolMode) {
		RemoteCall(new MiningSubscribeCall(*this));
	}
	else {
		RemoteCall(new LoginCall(*this, _apiKey));
	}
	return true;
}

void EquihashStratumWorker::PostSolution(core::Solution::Ref aSolution) const
{
	if (_poolMode) {
		const_cast<EquihashStratumWorker*>(this)->RemoteCall(new MiningSubmitCall(*const_cast<EquihashStratumWorker*>(this), aSolution));
	}
	else {
		const_cast<EquihashStratumWorker*>(this)->RemoteCall(new SolutionCall(*const_cast<EquihashStratumWorker*>(this), aSolution));
	}
}
