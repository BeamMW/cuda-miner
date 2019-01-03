#include "pch.hpp"
#include "base/BigInteger.hpp"
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/Logging.hpp"
#include "EquihashStratumWorker.hpp"
#include "BeamWork.hpp"
#include "core/Statistics.hpp"

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
						LOG(Info) << "New job #" << work->_id << " at difficulty " << work->_powDiff.ToFloat();
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
		std::string nonce;

		aBuffer += "{\"method\":\"solution\",\"id\":\"";
		aBuffer += _id;
		aBuffer += "\",\"nonce\":\"";
		_solution->PrintNonce(nonce, false);
		aBuffer += nonce;
		aBuffer += "\",\"output\":\"";
		_solution->PrintSolution(aBuffer);
		aBuffer += "\",\"jsonrpc\":\"2.0\"}\n";

		LOG(Info) << "Submitting solution for job #" << _id << " with nonce " << nonce;
	}

protected:
	core::StratumWorker		&_worker;
	core::Solution::Ref		_solution;
	core::Performance::Time	_start = std::chrono::system_clock::now();
};

//

EquihashStratumWorker::EquihashStratumWorker(const std::string &aApiKey) : _apiKey(aApiKey)
{
	Register(new JobNotify(*this));
	Register(new CancelNotify(*this));
}

bool EquihashStratumWorker::OnConnected()
{
	_connectedCounter++;
	RemoteCall(new LoginCall(*this, _apiKey));
	return true;
}

void EquihashStratumWorker::PostSolution(core::Solution::Ref aSolution) const
{
	const_cast<EquihashStratumWorker*>(this)->RemoteCall(new SolutionCall(*const_cast<EquihashStratumWorker*>(this), aSolution));
}
