#pragma	once

#include "base/BigInteger.hpp"
#include "core/Transport.hpp"
#include "core/Work.hpp"
#include "core/Solution.hpp"

namespace core
{
	class Worker : public Dynamic
	{
	public:
		typedef Reference<Worker> Ref;

	public:
		~Worker() override;

		virtual bool Start() = 0;
		virtual bool Stop() = 0;

		virtual Work::Ref GetWork() = 0;
		virtual void SetWork(Work::Ref aWork) = 0;

		virtual const std::string & GetServer() const;
		virtual void SetServer(const std::string &aServer);
		virtual const std::string & GetPort() const;
		virtual void SetPort(const std::string &aPort);
		virtual const std::string & GetUser() const;
		virtual void SetUser(const std::string &aUser);
		virtual const std::string & GetWorker() const;
		virtual void SetWorker(const std::string &aWorker);
		virtual const std::string & GetPassword() const;
		virtual void SetPassword(const std::string &aPassword);

		virtual bool IsConnected() const;
		virtual bool IsAuthorized() const;

		virtual void SetExtraNonce(const BigInteger &aNonce);
		virtual const BigInteger & GetExtraNonce() const;

		virtual void SetTarget(const BigInteger &aTarget);
		virtual const BigInteger & GetTarget() const;

		virtual void PostSolution(core::Solution::Ref aSolution) const = 0;

	protected:
		virtual bool OnCreateTransport() = 0;
		virtual bool OnConnected() = 0;
		virtual bool OnDisconnected() = 0;

	protected:
		std::string				_server;
		std::string				_port;
		std::string				_user;
		std::string				_worker;
		std::string				_password;
		core::Transport::Ref	_transport;
		bool					_connected = false;
		bool					_authorized = false;
		BigInteger				_target;
		BigInteger				_extraNonce;
	};
}
