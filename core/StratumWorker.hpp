#pragma	once

#include <thread>
#include <mutex>
#include "base/Signal.hpp"
#include "base/json.hpp"
#include "base/Semaphore.hpp"
#include "core/Worker.hpp"

namespace core
{
	class StratumWorker : public core::Worker
	{
	public:
		typedef Reference<StratumWorker> Ref;

		class Call : public Dynamic
		{
		public:
			typedef Reference<Call> Ref;
			typedef std::list<Call::Ref> List;
			typedef std::map<std::string, Call::Ref> Map;

		public:
			virtual const std::string & GetId() const;
			virtual const std::string & GetName() const;
			virtual bool OnCall(const JSonVar &aParameters);
			virtual bool OnResult(const JSonVar &aResult);
			virtual void OnTimeout();
			virtual bool HasResult() const;
			virtual bool HasParams() const;
			virtual void Serialize(std::string &aBuffer) const;

		protected:
			std::string	_id;
			std::string	_name;
		};


	public:
		StratumWorker();

		bool Start() override;
		bool Stop() override;

		Work::Ref GetWork() override;
		void SetWork(Work::Ref aWork) override;
		void CancelWork(const std::string &aId) override;
		bool IsCurrentWork(const core::Work &aWork) const override;

		bool Register(Call::Ref aMethod);
		long CreateCallId();

		virtual bool RemoteCall(Call::Ref aMethod);

	protected:
		bool OnCreateTransport() override;
		bool OnDisconnected() override;

		void Workflow();
		void Sender();

		virtual bool OnCall(const std::string &aId, const std::string &aName, const JSonVar &aCall);
		virtual bool OnNotify(const std::string &aName, const JSonVar &aCall);
		virtual bool OnResult(const std::string &aId, const JSonVar &aCall);

		bool ReadJson(JSonVar &aReply);

	public:
		static const std::string	kError;
		static const std::string	kId;
		static const std::string	kMethod;
		static const std::string	kParams;
		static const std::string	kResult;

	protected:
		StaticSignal		_eventSync;
		volatile bool		_exit = false;
		std::thread			_threadWorkflow;
		std::thread			_threadSender;
		Call::Map			_callHandlers;
		Call::Map			_pendingCalls;
		Call::List			_outputCalls;
		Semaphore			_semAvailable;
		std::mutex			_csOutput;
		std::atomic_long	_callId;
		PulseSignal			_eventWork;
		core::Work::Ref		_work;
		std::string			_buffer;
	};
}
